#pragma once

#include "can_types.h"
#include "control_can_native.h"

#include <QObject>
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

    void connectDevice(quint32 deviceIndex, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter);
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

private:
    void receiveLoop();
    QDateTime frameDateTime(const ControlCanNative::VciCanObj& frame, const QDateTime& fallbackTime);
    QString transmitErrorDetails(quint32 canId, int payloadSize, bool extendedFrame, bool remoteFrame, quint32 sent) const;
    void emitLog(const QString& message);

    quint32 deviceIndex_ = 0;
    quint32 channelIndex_ = 0;
    bool listenOnly_ = false;
    std::atomic_bool nmeaMonitorMode_ = false;
    bool hasTimestampBase_ = false;
    quint32 timestampBaseTicks_ = 0;
    qint64 timestampBaseSystemMs_ = 0;
    std::atomic_bool connected_ = false;
    std::atomic_bool stopReceive_ = false;
    std::thread receiveThread_;
    std::mutex nativeMutex_;
};
