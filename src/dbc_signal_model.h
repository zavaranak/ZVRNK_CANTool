#pragma once

#include "dbc.h"
#include "localization.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

// Latest physical value of every DBC signal seen on the bus, one row per
// signal in first-seen order.
class DbcSignalModel final : public QAbstractTableModel
{
public:
    explicit DbcSignalModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setDatabase(const Dbc::Database* database);
    void setLanguage(AppLanguage language);
    void setFilter(const QString& text);
    void update(int messageIndex, const QVector<Dbc::SignalValue>& values, const QString& timestamp, int seenCount);
    // Emits one change notification for all updates since the last call.
    void publish();
    void clear();

private:
    enum Column {
        MessageColumn,
        SignalColumn,
        ValueColumn,
        UnitColumn,
        RawColumn,
        CountColumn,
        UpdatedColumn,
        ColumnCount
    };

    struct Row
    {
        int messageIndex = -1;
        int signalIndex = -1;
        Dbc::SignalValue value;
        QString timestamp;
        int count = 0;
    };

    bool matchesFilter(const Row& row) const;
    void rebuildVisible();
    const Dbc::Signal& signalFor(const Row& row) const;

    const Dbc::Database* database_ = nullptr;
    QVector<Row> rows_;
    QHash<quint64, int> rowIndex_;
    QVector<int> visible_;
    QString filter_;
    bool rowsAdded_ = false;
    bool valuesChanged_ = false;
    AppLanguage language_ = AppLanguage::English;
};
