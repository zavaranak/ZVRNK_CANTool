#include "usb_can_service.h"

#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QThread>

#include <cstring>
#include <stdexcept>

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

void UsbCanService::connectDevice(quint32 deviceIndex, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter)
{
    if (connected_.load()) {
        throw std::runtime_error("Device is already connected.");
    }

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
    init.accCode = 0x00000000;
    init.accMask = 0xFFFFFFFF;
    init.reserved = 0;
    init.filter = singleFilter ? 0 : 1;
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

    deviceIndex_ = deviceIndex;
    channelIndex_ = channelIndex;
    listenOnly_ = listenOnly;
    hasTimestampBase_ = false;
    timestampBaseTicks_ = 0;
    timestampBaseSystemMs_ = 0;
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
            QVector<CanFrame> frames;
            frames.reserve(static_cast<int>(qMin<quint32>(received, 128)));
            QHash<QString, int> frameIndexByKey;
            for (quint64 i = 0; i < received; ++i) {
                const auto& raw = buffer[static_cast<int>(i)];
                const QDateTime now = QDateTime::currentDateTime();
                const QDateTime frameTime = frameDateTime(raw, now);
                QByteArray data(reinterpret_cast<const char*>(raw.data), raw.dataLen);
                CanFrame frame;
                frame.timestamp = frameTime.toString("HH:mm:ss.zzz");
                frame.firstSeenTimestamp = frame.timestamp;
                frame.lastSeenTimestamp = frame.timestamp;
                frame.firstSeenMs = frameTime.toMSecsSinceEpoch();
                frame.lastSeenMs = frame.firstSeenMs;
                frame.channel = QString::number(channelIndex_);
                frame.idText = formatId(raw.id, raw.externFlag == 1);
                frame.rawId = raw.id;
                frame.frameType = raw.remoteFlag == 1 ? "RTR" : (raw.externFlag == 1 ? "EXT" : "STD");
                frame.length = raw.dataLen;
                frame.dataBytes = data;
                frame.dataText = formatBytes(data);
                rawFrames.push_back(frame);

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

            if (!rawFrames.isEmpty()) {
                emit rawFramesReceived(rawFrames);
            }
            if (!frames.isEmpty()) {
                emit framesReceived(frames);
            }
        } catch (const std::exception& ex) {
            emitLog(QString("RX loop error: %1").arg(ex.what()));
            QThread::msleep(200);
        }
    }
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
