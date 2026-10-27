#pragma once

#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QString>
#include <QVector>

#include <optional>

// Vector CAN database (.dbc) messages, signals and value tables, plus
// decoding of raw frames into physical signal values.
namespace Dbc
{
struct Signal
{
    enum class Multiplex {
        None,
        Multiplexor,
        Multiplexed,
    };

    QString name;
    int startBit = 0;
    int length = 0;
    bool littleEndian = true; // @1 Intel; @0 Motorola
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    QString unit;
    Multiplex multiplex = Multiplex::None;
    qint64 multiplexValue = 0;
    QMap<qint64, QString> valueNames;
    QString comment;
};

struct Message
{
    quint32 id = 0;
    bool extended = false;
    QString name;
    int length = 8;
    QString transmitter;
    QVector<Signal> signalList;
    QString comment;
};

struct SignalValue
{
    int signalIndex = -1;
    qint64 raw = 0;
    double physical = 0.0;
};

class Database
{
public:
    bool loadFile(const QString& path, QString* error = nullptr);
    bool parse(const QString& text, QString* error = nullptr);
    void clear();

    bool isEmpty() const { return messages_.isEmpty(); }
    const QVector<Message>& messages() const { return messages_; }
    int signalCount() const;

    // Exact match first; with j1939Match, extended IDs also match while
    // ignoring the J1939 source address (lowest 8 bits).
    int findMessage(quint32 id, bool extended, bool j1939Match) const;

private:
    static quint64 key(quint32 id, bool extended);

    QVector<Message> messages_;
    QHash<quint64, int> index_;
    QHash<quint64, int> j1939Index_;
};

// Signals that are present in the payload; multiplexed signals are included
// only when the multiplexor selects them.
QVector<SignalValue> decode(const Message& message, const QByteArray& data);
std::optional<quint64> extractRaw(const Signal& signal, const QByteArray& data);
QString formatValue(const Signal& signal, const SignalValue& value);
QString formatSignals(const Message& message, const QVector<SignalValue>& values);
}
