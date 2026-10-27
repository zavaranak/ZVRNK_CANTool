#include "rx_frame_table_model.h"

#include <QStringList>
#include <utility>

RxFrameTableModel::RxFrameTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int RxFrameTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : frames_.size();
}

int RxFrameTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    switch (mode_) {
    case RxViewMode::Nmea:
        return 10;
    case RxViewMode::Dbc:
        return DbcColumnCount;
    default:
        return ColumnCount;
    }
}

QVariant RxFrameTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= frames_.size()) {
        return {};
    }

    const CanFrame& frame = frames_[index.row()];
    if (mode_ == RxViewMode::Dbc) {
        return dbcData(frame, index.column(), role);
    }
    if (mode_ == RxViewMode::Nmea) {
        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
            case 3:
            case 4:
            case 5:
                return int(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return int(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
        if (role != Qt::DisplayRole) {
            return {};
        }
        switch (index.column()) {
        case 0: return frame.timestamp;
        case 1: return frame.nmeaPgnText;
        case 2: return frame.nmeaName;
        case 3: return frame.nmeaPriority >= 0 ? QVariant(frame.nmeaPriority) : QVariant(QStringLiteral("-"));
        case 4: return frame.nmeaSource >= 0 ? QVariant(frame.nmeaSource) : QVariant(QStringLiteral("-"));
        case 5:
            return frame.nmeaDestination == 255
                ? QVariant(QStringLiteral("GLOBAL"))
                : (frame.nmeaDestination >= 0 ? QVariant(frame.nmeaDestination) : QVariant(QStringLiteral("-")));
        case 6: return frame.nmeaTransport;
        case 7: return frame.nmeaDecoded.isEmpty() ? QVariant(QStringLiteral("-")) : QVariant(frame.nmeaDecoded);
        case 8: return frame.idText;
        case 9: return frame.dataText;
        default: return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case SeenColumn:
        case FrequencyColumn:
        case ChannelColumn:
        case LengthColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case RealtimeColumn:
        return frame.timestamp;
    case SeenColumn:
        return frame.seenCount;
    case FrequencyColumn:
        return formatFrequency(frame.receiveFrequencyHz);
    case ChannelColumn:
        return frame.channel;
    case IdColumn:
        return frame.idText;
    case TypeColumn:
        return frame.frameType;
    case LengthColumn:
        return frame.length;
    case DataColumn:
        return frame.dataText;
    default:
        return {};
    }
}

QVariant RxFrameTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    const QStringList nmeaHeaders = {
        "Realtime",
        "PGN",
        "Name",
        "Priority",
        "Source",
        "Destination",
        "Transport",
        "Decoded",
        "ID",
        "Data"
    };
    static const QStringList faultHeaders = {
        "Realtime",
        "Seen",
        "FrequencyHz",
        "Channel",
        "ID",
        "Type",
        "Length",
        "Data"
    };

    static const QStringList dbcHeaders = {
        "Realtime",
        "Seen",
        "FrequencyHz",
        "Channel",
        "ID",
        "Message",
        "Length",
        "Data",
        "Signals"
    };

    const QStringList& headers = mode_ == RxViewMode::Nmea ? nmeaHeaders
        : (mode_ == RxViewMode::Dbc ? dbcHeaders : faultHeaders);
    return section >= 0 && section < headers.size()
        ? localize(language_, headers[section])
        : QVariant {};
}

void RxFrameTableModel::setFrames(QVector<CanFrame> frames)
{
    if (frames_.size() != frames.size()) {
        beginResetModel();
        frames_ = std::move(frames);
        endResetModel();
        return;
    }

    frames_ = std::move(frames);
    if (!frames_.isEmpty()) {
        emit dataChanged(index(0, 0), index(frames_.size() - 1, columnCount() - 1), {Qt::DisplayRole, Qt::TextAlignmentRole});
    }
}

void RxFrameTableModel::clear()
{
    setFrames({});
}

void RxFrameTableModel::setMode(RxViewMode mode)
{
    if (mode_ == mode) {
        return;
    }

    beginResetModel();
    mode_ = mode;
    endResetModel();
}

void RxFrameTableModel::setDbc(const Dbc::Database* database, bool j1939Match)
{
    beginResetModel();
    dbc_ = database;
    j1939Match_ = j1939Match;
    endResetModel();
}

int RxFrameTableModel::stretchColumn() const
{
    switch (mode_) {
    case RxViewMode::Dbc:
        return DbcSignalsColumn;
    case RxViewMode::Nmea:
        return 7;
    default:
        return DataColumn;
    }
}

QVariant RxFrameTableModel::dbcData(const CanFrame& frame, int column, int role) const
{
    if (role == Qt::TextAlignmentRole) {
        switch (column) {
        case DbcSeenColumn:
        case DbcFrequencyColumn:
        case DbcChannelColumn:
        case DbcLengthColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
        return {};
    }

    // Decoding only the cells on screen keeps large DBCs cheap.
    const int messageIndex = dbc_ ? dbc_->findMessage(frame.rawId, frame.frameType == "EXT", j1939Match_) : -1;
    const Dbc::Message* message = messageIndex >= 0 ? &dbc_->messages()[messageIndex] : nullptr;

    if (role == Qt::ToolTipRole) {
        if (!message || (column != DbcSignalsColumn && column != DbcMessageColumn)) {
            return {};
        }
        QStringList lines;
        lines << message->name;
        if (!message->comment.isEmpty()) {
            lines << message->comment;
        }
        for (const Dbc::SignalValue& value : Dbc::decode(*message, frame.dataBytes)) {
            const Dbc::Signal& signal = message->signalList[value.signalIndex];
            lines << QString("%1 = %2 %3  (raw %4)").arg(signal.name, Dbc::formatValue(signal, value), signal.unit).arg(value.raw);
        }
        return lines.join('\n');
    }

    switch (column) {
    case DbcRealtimeColumn:
        return frame.timestamp;
    case DbcSeenColumn:
        return frame.seenCount;
    case DbcFrequencyColumn:
        return formatFrequency(frame.receiveFrequencyHz);
    case DbcChannelColumn:
        return frame.channel;
    case DbcIdColumn:
        return frame.idText;
    case DbcMessageColumn:
        return message ? message->name : QStringLiteral("-");
    case DbcLengthColumn:
        return frame.length;
    case DbcDataColumn:
        return frame.dataText;
    case DbcSignalsColumn:
        return message ? Dbc::formatSignals(*message, Dbc::decode(*message, frame.dataBytes)) : QString();
    default:
        return {};
    }
}

void RxFrameTableModel::setLanguage(AppLanguage language)
{
    if (language_ == language) {
        return;
    }

    language_ = language;
    if (columnCount() > 0) {
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    }
}

QString RxFrameTableModel::formatFrequency(double hz)
{
    return QString::number(hz, 'f', 2);
}
