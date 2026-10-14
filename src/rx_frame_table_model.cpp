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
    return parent.isValid() ? 0 : (nmeaMode_ ? 10 : ColumnCount);
}

QVariant RxFrameTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= frames_.size()) {
        return {};
    }

    const CanFrame& frame = frames_[index.row()];
    if (nmeaMode_) {
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

    const QStringList& headers = nmeaMode_ ? nmeaHeaders : faultHeaders;
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

void RxFrameTableModel::setNmeaMode(bool enabled)
{
    if (nmeaMode_ == enabled) {
        return;
    }

    beginResetModel();
    nmeaMode_ = enabled;
    endResetModel();
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
