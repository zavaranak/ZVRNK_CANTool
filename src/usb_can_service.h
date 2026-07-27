#pragma once

#include "can_types.h"
#include "control_can_native.h"

#include <QObject>

#include <atomic>
#include <mutex>
#include <thread>

class UsbCanService final : public QObject
{
    Q_OBJECT

public:
    explicit UsbCanService(QObject* parent = nullptr);
    ~UsbCanService() override;

    bool isConnected() const;

    void connectDevice(quint32 deviceIndex, quint32 channelIndex, const QString& baudRate, bool listenOnly, bool singleFilter);
    void disconnectDevice();
    void sendFrame(quint32 canId, const QByteArray& payload, bool extendedFrame, bool remoteFrame);

    static QByteArray parsePayload(const QString& text);
    static quint32 parseCanId(const QString& text);
    static QString formatBytes(const QByteArray& bytes);
    static QString formatId(quint32 canId, bool extended);

signals:
    void frameReceived(const CanFrame& frame);
    void logEmitted(const QString& message);

private:
    void receiveLoop();
    void emitLog(const QString& message);

    quint32 deviceIndex_ = 0;
    quint32 channelIndex_ = 0;
    std::atomic_bool connected_ = false;
    std::atomic_bool stopReceive_ = false;
    std::thread receiveThread_;
    std::mutex nativeMutex_;
};
