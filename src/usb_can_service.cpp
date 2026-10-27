#include "usb_can_service.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {

quint32 bitrateFromText(const QString& baudRate)
{
    if (baudRate == "1M") {
        return 1000000;
    }
    if (baudRate == "500K") {
        return 500000;
    }
    if (baudRate == "250K") {
        return 250000;
    }
    if (baudRate == "125K") {
        return 125000;
    }
    throw std::runtime_error("Unsupported baud rate.");
}

// USB vendor IDs of bridges that GVRET boards commonly use; listed first.
bool isLikelyGvretPort(const QSerialPortInfo& port)
{
    if (!port.hasVendorIdentifier()) {
        return false;
    }
    switch (port.vendorIdentifier()) {
    case 0x303A: // Espressif native USB (ESP32-C3/S3)
    case 0x10C4: // Silicon Labs CP210x
    case 0x1A86: // WCH CH340/CH9102
    case 0x0403: // FTDI
    case 0x2341: // Arduino (Due / Macchina M2)
        return true;
    default:
        return false;
    }
}

}

UsbCanService::UsbCanService(QObject* parent)
    : QObject(parent)
{
}

UsbCanService::~UsbCanService()
{
    disconnectDevice();
}

bool UsbCanService::isConnected() const
{
    return connected_.load();
}

bool UsbCanService::isNmeaMonitorMode() const
{
    return nmeaMonitorMode_.load();
}

void UsbCanService::setNmeaMonitorMode(bool enabled)
{
    nmeaMonitorMode_.store(enabled);
}

QVector<CanDeviceInfo> UsbCanService::findDevices(QString* controlCanError)
{
    QVector<CanDeviceInfo> devices = ControlCanNative::instance().findDevices(controlCanError);

    auto ports = QSerialPortInfo::availablePorts();
    std::stable_sort(ports.begin(), ports.end(), [](const QSerialPortInfo& a, const QSerialPortInfo& b) {
        return isLikelyGvretPort(a) && !isLikelyGvretPort(b);
    });
    for (const QSerialPortInfo& port : ports) {
        // Bluetooth SPP links are never GVRET adapters, and opening one can
        // block while Windows tries to reach the paired device.
        if (port.description().contains("Bluetooth", Qt::CaseInsensitive)) {
            continue;
        }
        CanDeviceInfo device;
        device.backend = CanBackend::Gvret;
        device.portName = port.portName();
        device.hardwareType = port.description().isEmpty() ? QStringLiteral("Serial port") : port.description();
        device.serialNumber = port.serialNumber();
        device.channelCount = 1;
        devices.push_back(device);
    }
    return devices;
}

void UsbCanService::connectDevice(const CanDeviceInfo& device, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter)
{
    if (connected_.load()) {
        throw std::runtime_error("Device is already connected.");
    }

    hasTimestampBase_ = false;
    timestampBaseTicks_ = 0;
    timestampBaseSystemMs_ = 0;
    lastTimestampTicks_ = 0;
    elapsedTimestampUs_ = 0;

    if (device.backend == CanBackend::Gvret) {
        if (singleFilter) {
            emitLog("GVRET adapters have no hardware filter; accepting all identifiers.");
        }
        connectGvret(device.portName, channelIndex, baudRate, listenOnly);
    } else {
        connectControlCan(device.deviceIndex, channelIndex, baudRate, listenOnly, singleFilter);
    }
}

void UsbCanService::connectGvret(const QString& portName, quint32 channelIndex, const QString& baudRate, bool listenOnly)
{
    const quint32 bitrate = bitrateFromText(baudRate);
    if (channelIndex > 1) {
        throw std::runtime_error("GVRET supports channel 0 or 1.");
    }

    auto port = std::make_unique<QSerialPort>();
    port->setPortName(portName);
    port->setBaudRate(gvret::SerialBaudRate);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);
    if (!port->open(QIODevice::ReadWrite)) {
        throw std::runtime_error(QString("Could not open %1: %2. Close SavvyCAN or any other program using the port.")
                                     .arg(portName, port->errorString())
                                     .toStdString());
    }
    // ESP32 boards wire DTR/RTS to reset and boot-mode selection; holding
    // both low keeps the firmware running, matching SavvyCAN's ESP32 mode.
    port->setDataTerminalReady(false);
    port->setRequestToSend(false);
    port->clear();

    // The board may reboot when the port opens, so keep asking until the
    // firmware answers the device-info request.
    gvret::Parser parser;
    gvret::ParseResult result;
    QElapsedTimer elapsed;
    elapsed.start();
    qint64 nextRequestMs = 0;
    while (!result.deviceInfo && elapsed.elapsed() < 4000) {
        if (elapsed.elapsed() >= nextRequestMs) {
            port->write(gvret::handshake());
            port->waitForBytesWritten(100);
            nextRequestMs = elapsed.elapsed() + 300;
        }
        if (port->waitForReadyRead(50)) {
            parser.feed(port->readAll(), result);
        }
    }
    if (!result.deviceInfo) {
        port->close();
        throw std::runtime_error(QString("No GVRET response on %1. Check that this port is the GVRET adapter and that its firmware is running.")
                                     .arg(portName)
                                     .toStdString());
    }

    port->write(gvret::setupBus(static_cast<quint8>(channelIndex), bitrate, listenOnly));
    port->waitForBytesWritten(100);

    // Confirm the bus came up. Firmware that does not answer is logged
    // rather than treated as a failure.
    result.busParams.reset();
    QThread::msleep(50);
    port->write(gvret::command(gvret::GetCanBusParams));
    port->waitForBytesWritten(100);
    elapsed.restart();
    while (!result.busParams && elapsed.elapsed() < 1000) {
        if (port->waitForReadyRead(50)) {
            parser.feed(port->readAll(), result);
        }
    }
    if (!result.busParams) {
        emitLog("GVRET adapter did not report bus parameters; continuing.");
    } else if (channelIndex == 0) {
        if (!result.busParams->enabled) {
            port->close();
            throw std::runtime_error(QString("GVRET adapter on %1 failed to start CAN at %2.").arg(portName, baudRate).toStdString());
        }
        if (result.busParams->bitrate != bitrate) {
            emitLog(QString("GVRET adapter reports %1 bit/s instead of the requested %2 bit/s.")
                        .arg(result.busParams->bitrate)
                        .arg(bitrate));
        }
    }

    serial_ = port.release();
    serial_->setParent(this);
    gvretParser_ = parser;
    connect(serial_, &QSerialPort::readyRead, this, &UsbCanService::onSerialReadyRead);
    connect(serial_, &QSerialPort::errorOccurred, this, &UsbCanService::onSerialError);

    backend_ = CanBackend::Gvret;
    channelIndex_ = channelIndex;
    listenOnly_ = listenOnly;
    connected_.store(true);
    emitLog(QString("Connected GVRET %1 (build %2), channel %3, baud %4.")
                .arg(portName)
                .arg(result.deviceInfo->buildNumber)
                .arg(channelIndex)
                .arg(baudRate));
}

void UsbCanService::connectControlCan(quint32 deviceIndex, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter)
{
    quint8 timing0 = 0;
    quint8 timing1 = 0;
    if (baudRate == "1M") {
        timing0 = 0x00;
        timing1 = 0x14;
    } else if (baudRate == "500K") {
        timing0 = 0x00;
        timing1 = 0x1C;
    } else if (baudRate == "250K") {
        timing0 = 0x01;
        timing1 = 0x1C;
    } else if (baudRate == "125K") {
        timing0 = 0x03;
        timing1 = 0x1C;
    } else {
        throw std::runtime_error("Unsupported baud rate.");
    }

    QString loadError;
    auto& native = ControlCanNative::instance();
    if (!native.ensureLoaded(&loadError)) {
        throw std::runtime_error(loadError.toStdString());
    }

    ControlCanNative::VciInitConfig init {};
    // An all-ones mask with code 0 accepts only identifier 0 on many
    // ControlCAN adapters. NMEA monitoring must receive every identifier.
    const bool acceptAll = nmeaMonitorMode_.load() || !singleFilter;
    init.accCode = 0x00000000;
    init.accMask = acceptAll ? 0x00000000 : 0xFFFFFFFF;
    init.reserved = 0;
    init.filter = singleFilter ? 1 : 0;
    init.timing0 = timing0;
    init.timing1 = timing1;
    init.mode = listenOnly ? 1 : 0;

    std::lock_guard lock(nativeMutex_);
    if (native.openDevice(ControlCanNative::VciUsbCan2, deviceIndex, 0) != ControlCanNative::StatusOk) {
        throw std::runtime_error("VCI_OpenDevice failed.");
    }

    try {
        if (native.initCan(ControlCanNative::VciUsbCan2, deviceIndex, channelIndex, &init) != ControlCanNative::StatusOk) {
            throw std::runtime_error("VCI_InitCAN failed.");
        }
        if (native.startCan(ControlCanNative::VciUsbCan2, deviceIndex, channelIndex) != ControlCanNative::StatusOk) {
            throw std::runtime_error("VCI_StartCAN failed.");
        }
    } catch (...) {
        native.closeDevice(ControlCanNative::VciUsbCan2, deviceIndex);
        throw;
    }

    backend_ = CanBackend::ControlCan;
    deviceIndex_ = deviceIndex;
    channelIndex_ = channelIndex;
    listenOnly_ = listenOnly;
    stopReceive_.store(false);
    connected_.store(true);
    receiveThread_ = std::thread(&UsbCanService::receiveLoop, this);
    emitLog(QString("Connected device %1, channel %2, baud %3.").arg(deviceIndex).arg(channelIndex).arg(baudRate));
}

void UsbCanService::disconnectDevice()
{
    if (!connected_.exchange(false)) {
        return;
    }

    if (backend_ == CanBackend::Gvret) {
        if (serial_) {
            serial_->disconnect(this);
            serial_->close();
            serial_->deleteLater();
            serial_ = nullptr;
        }
        listenOnly_ = false;
        emitLog("Disconnected.");
        return;
    }

    stopReceive_.store(true);
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    std::lock_guard lock(nativeMutex_);
    auto& native = ControlCanNative::instance();
    native.resetCan(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_);
    native.closeDevice(ControlCanNative::VciUsbCan2, deviceIndex_);
    listenOnly_ = false;
    emitLog("Disconnected.");
}

void UsbCanService::sendFrame(quint32 canId, const QByteArray& payload, bool extendedFrame, bool remoteFrame)
{
    std::lock_guard lock(nativeMutex_);
    if (!connected_.load()) {
        throw std::runtime_error("Device is not connected.");
    }
    if (listenOnly_ || nmeaMonitorMode_.load()) {
        throw std::runtime_error("Transmit is disabled while the adapter is in receive-only monitoring mode.");
    }
    if (extendedFrame) {
        if (canId > 0x1FFFFFFF) {
            throw std::runtime_error("Extended CAN ID must be 29 bits or less.");
        }
    } else if (canId > 0x7FF) {
        throw std::runtime_error("Standard CAN ID must be 11 bits or less. Enable Extended for IDs above 0x7FF.");
    }
    if (payload.size() > 8) {
        throw std::runtime_error("CAN payload may contain at most 8 bytes.");
    }

    if (backend_ == CanBackend::Gvret) {
        if (remoteFrame) {
            throw std::runtime_error("GVRET adapters cannot transmit remote (RTR) frames.");
        }
        const QByteArray packet = gvret::encodeFrame(canId, extendedFrame, static_cast<quint8>(channelIndex_), payload);
        if (!serial_ || serial_->write(packet) != packet.size()) {
            throw std::runtime_error(QString("GVRET write failed: %1").arg(serial_ ? serial_->errorString() : QString("port closed")).toStdString());
        }
        emitLog(QString("TX %1 [%2] %3").arg(formatId(canId, extendedFrame)).arg(payload.size()).arg(formatBytes(payload)));
        return;
    }

    ControlCanNative::VciCanObj frame {};
    frame.id = canId;
    frame.sendType = 0;
    frame.remoteFlag = remoteFrame ? 1 : 0;
    frame.externFlag = extendedFrame ? 1 : 0;
    frame.dataLen = static_cast<quint8>(payload.size());
    std::memcpy(frame.data, payload.constData(), static_cast<size_t>(payload.size()));

    const quint32 sent = ControlCanNative::instance().transmit(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_, &frame, 1);
    if (sent != 1) {
        throw std::runtime_error(transmitErrorDetails(canId, payload.size(), extendedFrame, remoteFrame, sent).toStdString());
    }

    emitLog(QString("TX %1 [%2] %3").arg(formatId(canId, extendedFrame)).arg(payload.size()).arg(formatBytes(payload)));
}

QString UsbCanService::transmitErrorDetails(quint32 canId, int payloadSize, bool extendedFrame, bool remoteFrame, quint32 sent) const
{
    QString message = QString("VCI_Transmit failed (sent %1/1, device %2, channel %3, id %4, len %5, type %6, rtr %7).")
                          .arg(sent)
                          .arg(deviceIndex_)
                          .arg(channelIndex_)
                          .arg(formatId(canId, extendedFrame))
                          .arg(payloadSize)
                          .arg(extendedFrame ? "EXT" : "STD")
                          .arg(remoteFrame ? "yes" : "no");

    ControlCanNative::VciErrInfo errorInfo {};
    if (ControlCanNative::instance().readErrInfo(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_, &errorInfo)) {
        const auto hexByte = [](quint8 value) {
            return QString("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
        };
        message += QString(" Adapter error: code 0x%1, passive [%2 %3 %4], arbitration-lost 0x%5.")
                       .arg(errorInfo.errCode, 8, 16, QLatin1Char('0'))
                       .arg(hexByte(errorInfo.passiveErrData[0]))
                       .arg(hexByte(errorInfo.passiveErrData[1]))
                       .arg(hexByte(errorInfo.passiveErrData[2]))
                       .arg(hexByte(errorInfo.arLostErrData));
    }

    message += " Check Normal mode, matching baud rate, CAN-H/CAN-L wiring, termination, and that another node can ACK the frame.";
    return message;
}

QByteArray UsbCanService::parsePayload(const QString& text)
{
    QByteArray payload;
    const QStringList tokens = text.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
    if (tokens.size() > 8) {
        throw std::runtime_error("Payload supports at most 8 bytes.");
    }

    for (const QString& token : tokens) {
        bool ok = false;
        const int value = token.toInt(&ok, 16);
        if (!ok || value < 0 || value > 0xFF) {
            throw std::runtime_error("Payload bytes must be hex values from 00 to FF.");
        }
        payload.append(static_cast<char>(value));
    }
    return payload;
}

quint32 UsbCanService::parseCanId(const QString& text)
{
    QString value = text.trimmed();
    if (value.startsWith("0x", Qt::CaseInsensitive)) {
        value = value.mid(2);
    }
    bool ok = false;
    const quint32 id = value.toUInt(&ok, 16);
    if (!ok) {
        throw std::runtime_error("CAN ID must be hexadecimal.");
    }
    return id;
}

QString UsbCanService::formatBytes(const QByteArray& bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (const unsigned char value : bytes) {
        parts << QString("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
    }
    return parts.join(' ');
}

QString UsbCanService::formatId(quint32 canId, bool extended)
{
    return QString("0x%1").arg(canId, extended ? 8 : 3, 16, QLatin1Char('0')).toUpper();
}

void UsbCanService::receiveLoop()
{
    while (!stopReceive_.load()) {
        try {
            quint32 available = 0;
            {
                std::lock_guard lock(nativeMutex_);
                available = ControlCanNative::instance().getReceiveNum(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_);
            }

            if (available == 0) {
                QThread::msleep(10);
                continue;
            }

            const int fetchCount = static_cast<int>(qMin<quint32>(available, 512));
            QVector<ControlCanNative::VciCanObj> buffer(fetchCount);
            quint32 received = 0;
            {
                std::lock_guard lock(nativeMutex_);
                received = ControlCanNative::instance().receive(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_, buffer.data(), fetchCount, 50);
            }

            QVector<CanFrame> rawFrames;
            rawFrames.reserve(static_cast<int>(qMin<quint32>(received, 512)));
            for (quint64 i = 0; i < received; ++i) {
                const auto& raw = buffer[static_cast<int>(i)];
                const QDateTime frameTime = frameDateTime(raw, QDateTime::currentDateTime());
                const QByteArray data(reinterpret_cast<const char*>(raw.data), raw.dataLen);
                rawFrames.push_back(makeFrame(frameTime, channelIndex_, raw.id, raw.externFlag == 1, raw.remoteFlag == 1, data));
            }
            publishFrames(rawFrames);
        } catch (const std::exception& ex) {
            emitLog(QString("RX loop error: %1").arg(ex.what()));
            QThread::msleep(200);
        }
    }
}

CanFrame UsbCanService::makeFrame(const QDateTime& frameTime, quint32 channel, quint32 id, bool extended, bool remote, const QByteArray& data) const
{
    CanFrame frame;
    frame.timestamp = frameTime.toString("HH:mm:ss.zzz");
    frame.firstSeenTimestamp = frame.timestamp;
    frame.lastSeenTimestamp = frame.timestamp;
    frame.firstSeenMs = frameTime.toMSecsSinceEpoch();
    frame.lastSeenMs = frame.firstSeenMs;
    frame.channel = QString::number(channel);
    frame.idText = formatId(id, extended);
    frame.rawId = id;
    frame.frameType = remote ? "RTR" : (extended ? "EXT" : "STD");
    frame.length = data.size();
    frame.dataBytes = data;
    frame.dataText = formatBytes(data);
    return frame;
}

void UsbCanService::publishFrames(const QVector<CanFrame>& rawFrames)
{
    if (rawFrames.isEmpty()) {
        return;
    }

    QVector<CanFrame> frames;
    frames.reserve(qMin<int>(rawFrames.size(), 128));
    QHash<QString, int> frameIndexByKey;
    for (const CanFrame& frame : rawFrames) {
        const QString key = QString("%1|%2|%3").arg(frame.channel, QString::number(frame.rawId, 16), frame.frameType);
        const auto existing = frameIndexByKey.constFind(key);
        if (existing == frameIndexByKey.cend()) {
            frameIndexByKey.insert(key, frames.size());
            frames.push_back(frame);
        } else {
            const int existingIndex = existing.value();
            const int batchSeen = frames[existingIndex].seenCount + 1;
            frames[existingIndex] = frame;
            frames[existingIndex].seenCount = batchSeen;
        }
    }

    emit rawFramesReceived(rawFrames);
    emit framesReceived(frames);
}

void UsbCanService::onSerialReadyRead()
{
    if (!serial_ || !connected_.load()) {
        return;
    }

    gvret::ParseResult result;
    gvretParser_.feed(serial_->readAll(), result);

    QVector<CanFrame> rawFrames;
    rawFrames.reserve(result.frames.size());
    const QDateTime now = QDateTime::currentDateTime();
    for (const gvret::Frame& raw : result.frames) {
        rawFrames.push_back(makeFrame(gvretFrameDateTime(raw.timestampUs, now), raw.bus, raw.id, raw.extended, false, raw.data));
    }
    publishFrames(rawFrames);
}

void UsbCanService::onSerialError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::ResourceError && error != QSerialPort::PermissionError) {
        return;
    }

    emitLog(QString("GVRET serial port lost: %1").arg(serial_ ? serial_->errorString() : QString()));
    // Closing the port from inside its own error signal is unsafe; defer it.
    QTimer::singleShot(0, this, [this] {
        if (connected_.load() && backend_ == CanBackend::Gvret) {
            disconnectDevice();
            emit connectionLost();
        }
    });
}

QDateTime UsbCanService::gvretFrameDateTime(quint32 timestampUs, const QDateTime& fallbackTime)
{
    const qint64 nowMs = fallbackTime.toMSecsSinceEpoch();
    if (hasTimestampBase_) {
        // Unsigned subtraction keeps working across the 32-bit wrap (~71 min).
        elapsedTimestampUs_ += static_cast<quint32>(timestampUs - lastTimestampTicks_);
        lastTimestampTicks_ = timestampUs;
        const qint64 frameMs = timestampBaseSystemMs_ + static_cast<qint64>(elapsedTimestampUs_ / 1000);
        // Rebase if the adapter rebooted or its clock drifted noticeably.
        if (qAbs(frameMs - nowMs) <= 2000) {
            return QDateTime::fromMSecsSinceEpoch(frameMs);
        }
    }

    hasTimestampBase_ = true;
    lastTimestampTicks_ = timestampUs;
    elapsedTimestampUs_ = 0;
    timestampBaseSystemMs_ = nowMs;
    return fallbackTime;
}

QDateTime UsbCanService::frameDateTime(const ControlCanNative::VciCanObj& frame, const QDateTime& fallbackTime)
{
    if (frame.timeFlag == 0) {
        return fallbackTime;
    }

    if (!hasTimestampBase_) {
        hasTimestampBase_ = true;
        timestampBaseTicks_ = frame.timeStamp;
        timestampBaseSystemMs_ = fallbackTime.toMSecsSinceEpoch();
        return fallbackTime;
    }

    const quint32 elapsedTicks = frame.timeStamp - timestampBaseTicks_;
    const qint64 elapsedMs = static_cast<qint64>(elapsedTicks) / 10;
    return QDateTime::fromMSecsSinceEpoch(timestampBaseSystemMs_ + elapsedMs);
}

void UsbCanService::emitLog(const QString& message)
{
    emit logEmitted(QString("%1  %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
}
