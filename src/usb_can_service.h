#pragma once

#include "can_types.h"
#include "control_can_native.h"
#include "gvret_protocol.h"

#include <QObject>
#include <QSerialPort>
#include <QVector>

#include <atomic>
#include <mutex>
#include <thread>

class QDateTime;

class UsbCanService final : public QObject
{
    Q_OBJECT

public:
    explicit UsbCanService(QObject* parent = nullptr);
    ~UsbCanService() override;

    bool isConnected() const;
    bool isNmeaMonitorMode() const;

    // Lists ControlCAN adapters and serial ports that may carry GVRET. A
    // ControlCAN load failure is reported through controlCanError but does
    // not hide the serial ports.
    static QVector<CanDeviceInfo> findDevices(QString* controlCanError = nullptr);

    void connectDevice(const CanDeviceInfo& device, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter);
    void disconnectDevice();
    void setNmeaMonitorMode(bool enabled);
    void sendFrame(quint32 canId, const QByteArray& payload, bool extendedFrame, bool remoteFrame);

    static QByteArray parsePayload(const QString& text);
    static quint32 parseCanId(const QString& text);
    static QString formatBytes(const QByteArray& bytes);
    static QString formatId(quint32 canId, bool extended);

signals:
    void framesReceived(const QVector<CanFrame>& frames);
    void rawFramesReceived(const QVector<CanFrame>& frames);
    void logEmitted(const QString& message);
    // The adapter vanished (for example, the USB serial port was unplugged).
    void connectionLost();

private:
    void connectControlCan(quint32 deviceIndex, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter);
    void connectGvret(const QString& portName, quint32 channelIndex, const QString& baudRate, bool listenOnly);
    void receiveLoop();
    void onSerialReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void publishFrames(const QVector<CanFrame>& rawFrames);
    CanFrame makeFrame(const QDateTime& frameTime, quint32 channel, quint32 id, bool extended, bool remote, const QByteArray& data) const;
    QDateTime frameDateTime(const ControlCanNative::VciCanObj& frame, const QDateTime& fallbackTime);
    QDateTime gvretFrameDateTime(quint32 timestampUs, const QDateTime& fallbackTime);
    QString transmitErrorDetails(quint32 canId, int payloadSize, bool extendedFrame, bool remoteFrame, quint32 sent) const;
    void emitLog(const QString& message);

    CanBackend backend_ = CanBackend::ControlCan;
    quint32 deviceIndex_ = 0;
    quint32 channelIndex_ = 0;
    bool listenOnly_ = false;
    std::atomic_bool nmeaMonitorMode_ = false;
    bool hasTimestampBase_ = false;
    quint32 timestampBaseTicks_ = 0;
    qint64 timestampBaseSystemMs_ = 0;
    quint32 lastTimestampTicks_ = 0;
    quint64 elapsedTimestampUs_ = 0;
    std::atomic_bool connected_ = false;
    std::atomic_bool stopReceive_ = false;
    std::thread receiveThread_;
    std::mutex nativeMutex_;
    // GVRET lives on the GUI thread and is driven by readyRead.
    QSerialPort* serial_ = nullptr;
    gvret::Parser gvretParser_;
};
