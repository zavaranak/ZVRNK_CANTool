#include "dbc.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace Dbc
{
namespace {

constexpr quint32 DbcExtendedFlag = 0x80000000;
constexpr quint32 J1939SourceMask = 0x1FFFFF00;

// Splits the file into statements. Most end at a newline, but CM_ and VAL_
// may run over several lines until a ';' outside quotes.
QStringList statements(const QString& text)
{
    QStringList result;
    QString current;
    bool inQuotes = false;
    bool multiLine = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (current.isEmpty() && c.isSpace()) {
            continue;
        }
        if (current.isEmpty()) {
            // The NS_ section lists bare "CM_" / "VAL_" keywords on their own
            // lines; only treat them as statements when something follows.
            static const QRegularExpression multiLineStart(R"(^(CM_|VAL_)[ \t]+\S)");
            multiLine = multiLineStart.matchView(QStringView(text).mid(i, 64)).hasMatch();
        }
        if (c == '"' && (i == 0 || text[i - 1] != '\\')) {
            inQuotes = !inQuotes;
        }
        if (!inQuotes && ((multiLine && c == ';') || (!multiLine && (c == '\n' || c == '\r')))) {
            result.push_back(current.trimmed());
            current.clear();
            continue;
        }
        current.append(c);
    }
    if (!current.trimmed().isEmpty()) {
        result.push_back(current.trimmed());
    }
    return result;
}

QString unquote(const QString& text)
{
    QString value = text;
    value.replace("\\\"", "\"");
    return value;
}

}

bool Database::loadFile(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    // DBC files are usually Windows-1252; accept UTF-8 when it decodes cleanly.
    const QByteArray bytes = file.readAll();
    QString text = QString::fromUtf8(bytes);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLatin1(bytes);
    }
    return parse(text, error);
}

bool Database::parse(const QString& text, QString* error)
{
    static const QRegularExpression messagePattern(
        R"(^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s*(\w*))");
    static const QRegularExpression signalPattern(
        R"(^SG_\s+(\w+)\s*(M|m\d+M?)?\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*\(\s*([^,\s]+)\s*,\s*([^)\s]+)\s*\)\s*\[\s*([^|\s]*)\s*\|\s*([^\]\s]*)\s*\]\s*"((?:[^"\\]|\\.)*)\")");
    static const QRegularExpression valuePattern(R"((-?\d+)\s+"((?:[^"\\]|\\.)*)\")");
    static const QRegularExpression valueHeaderPattern(R"(^VAL_\s+(\d+)\s+(\w+)\s+(.*)$)",
                                                       QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression messageCommentPattern(R"(^CM_\s+BO_\s+(\d+)\s+"((?:[^"\\]|\\.)*)\")",
                                                          QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression signalCommentPattern(R"(^CM_\s+SG_\s+(\d+)\s+(\w+)\s+"((?:[^"\\]|\\.)*)\")",
                                                         QRegularExpression::DotMatchesEverythingOption);

    clear();
    QHash<quint32, int> byRawId;
    const auto signalByName = [this, &byRawId](quint32 rawId, const QString& name) -> Signal* {
        const auto message = byRawId.constFind(rawId);
        if (message == byRawId.cend()) {
            return nullptr;
        }
        for (Signal& signal : messages_[message.value()].signalList) {
            if (signal.name == name) {
                return &signal;
            }
        }
        return nullptr;
    };

    Message* current = nullptr;
    for (const QString& statement : statements(text)) {
        if (statement.startsWith("BO_ ")) {
            const auto match = messagePattern.match(statement);
            if (!match.hasMatch()) {
                current = nullptr;
                continue;
            }
            const quint32 rawId = match.captured(1).toUInt();
            Message message;
            message.extended = (rawId & DbcExtendedFlag) != 0;
            message.id = rawId & 0x1FFFFFFF;
            message.name = match.captured(2);
            message.length = match.captured(3).toInt();
            message.transmitter = match.captured(4);
            byRawId.insert(rawId, messages_.size());
            messages_.push_back(message);
            current = &messages_.last();
        } else if (statement.startsWith("SG_ ")) {
            if (!current) {
                continue;
            }
            const auto match = signalPattern.match(statement);
            if (!match.hasMatch()) {
                continue;
            }
            Signal signal;
            signal.name = match.captured(1);
            const QString multiplex = match.captured(2);
            if (multiplex == "M") {
                signal.multiplex = Signal::Multiplex::Multiplexor;
            } else if (multiplex.startsWith('m')) {
                signal.multiplex = Signal::Multiplex::Multiplexed;
                signal.multiplexValue = multiplex.mid(1).remove('M').toLongLong();
            }
            signal.startBit = match.captured(3).toInt();
            signal.length = match.captured(4).toInt();
            signal.littleEndian = match.captured(5) == "1";
            signal.isSigned = match.captured(6) == "-";
            signal.factor = match.captured(7).toDouble();
            signal.offset = match.captured(8).toDouble();
            signal.minimum = match.captured(9).toDouble();
            signal.maximum = match.captured(10).toDouble();
            signal.unit = unquote(match.captured(11));
            if (signal.length <= 0 || signal.length > 64) {
                continue;
            }
            current->signalList.push_back(signal);
        } else if (statement.startsWith("VAL_ ") || statement.startsWith("VAL_\t")) {
            current = nullptr;
            const auto header = valueHeaderPattern.match(statement);
            if (!header.hasMatch()) {
                continue;
            }
            Signal* signal = signalByName(header.captured(1).toUInt(), header.captured(2));
            if (!signal) {
                continue;
            }
            auto values = valuePattern.globalMatch(header.captured(3));
            while (values.hasNext()) {
                const auto value = values.next();
                signal->valueNames.insert(value.captured(1).toLongLong(), unquote(value.captured(2)));
            }
        } else if (statement.startsWith("CM_")) {
            current = nullptr;
            const auto signalComment = signalCommentPattern.match(statement);
            if (signalComment.hasMatch()) {
                if (Signal* signal = signalByName(signalComment.captured(1).toUInt(), signalComment.captured(2))) {
                    signal->comment = unquote(signalComment.captured(3));
                }
                continue;
            }
            const auto messageComment = messageCommentPattern.match(statement);
            if (messageComment.hasMatch()) {
                const auto message = byRawId.constFind(messageComment.captured(1).toUInt());
                if (message != byRawId.cend()) {
                    messages_[message.value()].comment = unquote(messageComment.captured(2));
                }
            }
        } else if (!statement.startsWith("SG_")) {
            current = nullptr;
        }
    }

    // Vector tools store a placeholder message for unassigned signals.
    messages_.erase(std::remove_if(messages_.begin(), messages_.end(), [](const Message& message) {
        return message.name == "VECTOR__INDEPENDENT_SIG_MSG";
    }), messages_.end());

    for (int i = 0; i < messages_.size(); ++i) {
        const Message& message = messages_[i];
        index_.insert(key(message.id, message.extended), i);
        if (message.extended) {
            j1939Index_.insert(message.id & J1939SourceMask, i);
        }
    }

    if (messages_.isEmpty()) {
        if (error) {
            *error = "No BO_ message definitions found.";
        }
        return false;
    }
    return true;
}

void Database::clear()
{
    messages_.clear();
    index_.clear();
    j1939Index_.clear();
}

int Database::signalCount() const
{
    int count = 0;
    for (const Message& message : messages_) {
        count += message.signalList.size();
    }
    return count;
}

int Database::findMessage(quint32 id, bool extended, bool j1939Match) const
{
    const auto exact = index_.constFind(key(id, extended));
    if (exact != index_.cend()) {
        return exact.value();
    }
    if (j1939Match && extended) {
        const auto j1939 = j1939Index_.constFind(id & J1939SourceMask);
        if (j1939 != j1939Index_.cend()) {
            return j1939.value();
        }
    }
    return -1;
}

quint64 Database::key(quint32 id, bool extended)
{
    return (static_cast<quint64>(extended ? 1 : 0) << 32) | id;
}

std::optional<quint64> extractRaw(const Signal& signal, const QByteArray& data)
{
    const int totalBits = data.size() * 8;
    quint64 raw = 0;
    if (signal.littleEndian) {
        // Intel: start bit is the LSB; bits run upward through the bytes.
        if (signal.startBit + signal.length > totalBits) {
            return std::nullopt;
        }
        for (int i = signal.length - 1; i >= 0; --i) {
            const int bit = signal.startBit + i;
            raw = (raw << 1) | ((static_cast<quint8>(data[bit / 8]) >> (bit % 8)) & 1);
        }
        return raw;
    }

    // Motorola: start bit is the MSB in sawtooth numbering; walk toward the
    // LSB, wrapping from bit 0 of one byte to bit 7 of the next.
    int bit = signal.startBit;
    for (int i = 0; i < signal.length; ++i) {
        if (bit < 0 || bit >= totalBits) {
            return std::nullopt;
        }
        raw = (raw << 1) | ((static_cast<quint8>(data[bit / 8]) >> (bit % 8)) & 1);
        bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
    }
    return raw;
}

QVector<SignalValue> decode(const Message& message, const QByteArray& data)
{
    QVector<SignalValue> values;
    values.reserve(message.signalList.size());

    std::optional<qint64> multiplexor;
    for (const Signal& signal : message.signalList) {
        if (signal.multiplex == Signal::Multiplex::Multiplexor) {
            if (const auto raw = extractRaw(signal, data)) {
                multiplexor = static_cast<qint64>(*raw);
            }
            break;
        }
    }

    for (int i = 0; i < message.signalList.size(); ++i) {
        const Signal& signal = message.signalList[i];
        if (signal.multiplex == Signal::Multiplex::Multiplexed
            && (!multiplexor || *multiplexor != signal.multiplexValue)) {
            continue;
        }
        const auto rawBits = extractRaw(signal, data);
        if (!rawBits) {
            continue;
        }

        qint64 raw = static_cast<qint64>(*rawBits);
        if (signal.isSigned && signal.length < 64 && (*rawBits >> (signal.length - 1)) & 1) {
            raw -= static_cast<qint64>(1ULL << signal.length);
        }

        SignalValue value;
        value.signalIndex = i;
        value.raw = raw;
        value.physical = static_cast<double>(raw) * signal.factor + signal.offset;
        values.push_back(value);
    }
    return values;
}

QString formatValue(const Signal& signal, const SignalValue& value)
{
    const auto name = signal.valueNames.constFind(value.raw);
    if (name != signal.valueNames.cend()) {
        return name.value();
    }

    // Show as many decimals as the scaling factor can produce.
    int decimals = 0;
    const double step = std::fabs(signal.factor);
    if (step > 0.0 && step < 1.0) {
        decimals = qBound(0, static_cast<int>(std::ceil(-std::log10(step) - 1e-9)), 6);
    }
    if (std::fmod(std::fabs(signal.offset), 1.0) > 1e-9) {
        decimals = qMax(decimals, 1);
    }
    return QString::number(value.physical, 'f', decimals);
}

QString formatSignals(const Message& message, const QVector<SignalValue>& values)
{
    QStringList parts;
    parts.reserve(values.size());
    for (const SignalValue& value : values) {
        const Signal& signal = message.signalList[value.signalIndex];
        QString text = QString("%1=%2").arg(signal.name, formatValue(signal, value));
        if (!signal.unit.isEmpty() && !signal.valueNames.contains(value.raw)) {
            text += ' ' + signal.unit;
        }
        parts.push_back(text);
    }
    return parts.join("  ");
}
}
