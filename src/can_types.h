#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

struct CanDeviceInfo
{
    quint32 deviceIndex = 0;
    QString serialNumber;
    QString hardwareType;
    quint8 channelCount = 0;

    QString displayName() const
    {
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
    QString channel;
    QString idText;
    quint32 rawId = 0;
    QString frameType;
    int length = 0;
    QByteArray dataBytes;
    QString dataText;
    int seenCount = 1;
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
