#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

enum class CanBackend
{
    ControlCan,
    Gvret,
};

struct CanDeviceInfo
{
    CanBackend backend = CanBackend::ControlCan;
    quint32 deviceIndex = 0;
    QString serialNumber;
    QString hardwareType;
    quint8 channelCount = 0;
    // GVRET serial adapters only.
    QString portName;

    QString key() const
    {
        return backend == CanBackend::Gvret
            ? QString("gvret:%1").arg(portName)
            : QString("vci:%1").arg(deviceIndex);
    }

    QString shortName() const
    {
        return backend == CanBackend::Gvret ? portName : QString("Dev %1").arg(deviceIndex);
    }

    QString displayName() const
    {
        if (backend == CanBackend::Gvret) {
            return QString("GVRET  %1  %2").arg(portName, hardwareType);
        }
        return QString("#%1  SN:%2  %3  CH:%4")
            .arg(deviceIndex)
            .arg(serialNumber)
            .arg(hardwareType)
            .arg(channelCount);
    }
};

struct CanFrame
{
    QString timestamp;
    QString firstSeenTimestamp;
    QString lastSeenTimestamp;
    qint64 firstSeenMs = 0;
    qint64 lastSeenMs = 0;
    double receiveFrequencyHz = 0.0;
    QString channel;
    QString idText;
    quint32 rawId = 0;
    QString frameType;
    int length = 0;
    QByteArray dataBytes;
    QString dataText;
    int seenCount = 1;
    bool nmeaValid = false;
    quint32 nmeaPgn = 0;
    QString nmeaPgnText;
    QString nmeaName;
    int nmeaPriority = -1;
    int nmeaSource = -1;
    int nmeaDestination = -1;
    QString nmeaTransport;
    QString nmeaDecoded;
};

struct TxFrame
{
    QString name = "F1";
    QString idText = "0x000";
    QString payloadText = "00";
    bool extendedFrame = false;
    bool remoteFrame = false;
    bool cycleEnabled = false;
    int intervalMs = 200;
};

Q_DECLARE_METATYPE(CanFrame)
