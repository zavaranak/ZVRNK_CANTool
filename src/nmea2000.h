#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace Nmea2000
{
struct RawFrame
{
    qint64 timestampMs = 0;
    QString timestamp;
    quint32 canId = 0;
    bool extended = false;
    QByteArray data;
};

struct Info
{
    bool valid = false;
    quint32 canId = 0;
    quint32 pgn = 0;
    int priority = -1;
    int source = -1;
    int destination = -1;
    QString pgnText;
    QString pgnName;
    QString transport;
    QString decoded;
    QByteArray payload;
};

class Decoder final
{
public:
    Info process(const RawFrame& frame);
    void reset();

private:
    struct FastPacketState
    {
        int expectedLength = 0;
        int nextFrame = 1;
        int sequence = 0;
        QByteArray payload;
    };

    QHash<QString, FastPacketState> fastPackets_;
};

QString pgnName(quint32 pgn);
bool isFastPacketPgn(quint32 pgn);
QString decodePayload(quint32 pgn, const QByteArray& payload);
}
