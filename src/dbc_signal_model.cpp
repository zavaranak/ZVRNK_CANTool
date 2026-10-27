#include "dbc_signal_model.h"

#include <QStringList>

DbcSignalModel::DbcSignalModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int DbcSignalModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : visible_.size();
}

int DbcSignalModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DbcSignalModel::data(const QModelIndex& index, int role) const
{
    if (!database_ || !index.isValid() || index.row() < 0 || index.row() >= visible_.size()) {
        return {};
    }

    const Row& row = rows_[visible_[index.row()]];
    const Dbc::Signal& signal = signalFor(row);
    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ValueColumn:
        case RawColumn:
        case CountColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (role == Qt::ToolTipRole) {
        const Dbc::Message& message = database_->messages()[row.messageIndex];
        QString tip = QString("%1 / %2\nRange %3 .. %4, factor %5, offset %6")
                          .arg(message.name, signal.name)
                          .arg(signal.minimum)
                          .arg(signal.maximum)
                          .arg(signal.factor)
                          .arg(signal.offset);
        if (!signal.comment.isEmpty()) {
            tip += '\n' + signal.comment;
        }
        return tip;
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case MessageColumn:
        return database_->messages()[row.messageIndex].name;
    case SignalColumn:
        return signal.name;
    case ValueColumn:
        return Dbc::formatValue(signal, row.value);
    case UnitColumn:
        return signal.unit;
    case RawColumn:
        return row.value.raw;
    case CountColumn:
        return row.count;
    case UpdatedColumn:
        return row.timestamp;
    default:
        return {};
    }
}

QVariant DbcSignalModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    static const QStringList headers = {"Message", "Signal", "Value", "Unit", "Raw", "Count", "Updated"};
    return section >= 0 && section < headers.size() ? localize(language_, headers[section]) : QVariant {};
}

void DbcSignalModel::setDatabase(const Dbc::Database* database)
{
    beginResetModel();
    database_ = database;
    rows_.clear();
    rowIndex_.clear();
    visible_.clear();
    rowsAdded_ = false;
    valuesChanged_ = false;
    endResetModel();
}

void DbcSignalModel::setLanguage(AppLanguage language)
{
    if (language_ == language) {
        return;
    }
    language_ = language;
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

void DbcSignalModel::setFilter(const QString& text)
{
    const QString filter = text.trimmed();
    if (filter == filter_) {
        return;
    }
    beginResetModel();
    filter_ = filter;
    rebuildVisible();
    endResetModel();
}

void DbcSignalModel::update(int messageIndex, const QVector<Dbc::SignalValue>& values, const QString& timestamp, int seenCount)
{
    for (const Dbc::SignalValue& value : values) {
        const quint64 key = (static_cast<quint64>(messageIndex) << 32) | static_cast<quint32>(value.signalIndex);
        auto existing = rowIndex_.find(key);
        if (existing == rowIndex_.end()) {
            Row row;
            row.messageIndex = messageIndex;
            row.signalIndex = value.signalIndex;
            existing = rowIndex_.insert(key, rows_.size());
            rows_.push_back(row);
            rowsAdded_ = true;
        }
        Row& row = rows_[existing.value()];
        row.value = value;
        row.timestamp = timestamp;
        row.count += qMax(1, seenCount);
        valuesChanged_ = true;
    }
}

void DbcSignalModel::publish()
{
    if (rowsAdded_) {
        beginResetModel();
        rebuildVisible();
        endResetModel();
    } else if (valuesChanged_ && !visible_.isEmpty()) {
        emit dataChanged(index(0, ValueColumn), index(visible_.size() - 1, UpdatedColumn), {Qt::DisplayRole});
    }
    rowsAdded_ = false;
    valuesChanged_ = false;
}

void DbcSignalModel::clear()
{
    setDatabase(database_);
}

bool DbcSignalModel::matchesFilter(const Row& row) const
{
    if (filter_.isEmpty()) {
        return true;
    }
    return signalFor(row).name.contains(filter_, Qt::CaseInsensitive)
        || database_->messages()[row.messageIndex].name.contains(filter_, Qt::CaseInsensitive);
}

void DbcSignalModel::rebuildVisible()
{
    visible_.clear();
    if (!database_) {
        return;
    }
    for (int i = 0; i < rows_.size(); ++i) {
        if (matchesFilter(rows_[i])) {
            visible_.push_back(i);
        }
    }
}

const Dbc::Signal& DbcSignalModel::signalFor(const Row& row) const
{
    return database_->messages()[row.messageIndex].signalList[row.signalIndex];
}
