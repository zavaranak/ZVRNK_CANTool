#include "usb_can_service.h"

#include <QDateTime>
#include <QRegularExpression>
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
    emitLog("Disconnected.");
}

void UsbCanService::sendFrame(quint32 canId, const QByteArray& payload, bool extendedFrame, bool remoteFrame)
{
    if (!connected_.load()) {
        throw std::runtime_error("Device is not connected.");
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

    std::lock_guard lock(nativeMutex_);
    const quint32 sent = ControlCanNative::instance().transmit(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_, &frame, 1);
    if (sent != 1) {
        throw std::runtime_error("VCI_Transmit failed.");
    }

    emitLog(QString("TX %1 [%2] %3").arg(formatId(canId, extendedFrame)).arg(payload.size()).arg(formatBytes(payload)));
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
                QThread::msleep(50);
                continue;
            }

            const int fetchCount = static_cast<int>(qMin<quint32>(available, 128));
            QVector<ControlCanNative::VciCanObj> buffer(fetchCount);
            quint32 received = 0;
            {
                std::lock_guard lock(nativeMutex_);
                received = ControlCanNative::instance().receive(ControlCanNative::VciUsbCan2, deviceIndex_, channelIndex_, buffer.data(), fetchCount, 50);
            }

            for (quint64 i = 0; i < received; ++i) {
                const auto& raw = buffer[static_cast<int>(i)];
                QByteArray data(reinterpret_cast<const char*>(raw.data), raw.dataLen);
                CanFrame frame;
                frame.timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
                frame.channel = QString::number(channelIndex_);
                frame.idText = formatId(raw.id, raw.externFlag == 1);
                frame.rawId = raw.id;
                frame.frameType = raw.remoteFlag == 1 ? "RTR" : (raw.externFlag == 1 ? "EXT" : "STD");
                frame.length = raw.dataLen;
                frame.dataBytes = data;
                frame.dataText = formatBytes(data);
                emit frameReceived(frame);
            }
        } catch (const std::exception& ex) {
            emitLog(QString("RX loop error: %1").arg(ex.what()));
            QThread::msleep(200);
        }
    }
}

void UsbCanService::emitLog(const QString& message)
{
    emit logEmitted(QString("%1  %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
}
