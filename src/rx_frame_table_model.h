#pragma once

#include "can_types.h"
#include "dbc.h"
#include "localization.h"

#include <QAbstractTableModel>
#include <QVector>

enum class RxViewMode
{
    Fault,
    Nmea,
    Dbc,
};

class RxFrameTableModel final : public QAbstractTableModel
{
public:
    explicit RxFrameTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setFrames(QVector<CanFrame> frames);
    void setMode(RxViewMode mode);
    RxViewMode mode() const { return mode_; }
    // The database must outlive the model or be cleared with nullptr.
    void setDbc(const Dbc::Database* database, bool j1939Match);
    // Column that should take the spare width in the current mode.
    int stretchColumn() const;
    void setLanguage(AppLanguage language);
    void clear();

private:
    enum Column {
        RealtimeColumn,
        SeenColumn,
        FrequencyColumn,
        ChannelColumn,
        IdColumn,
        TypeColumn,
        LengthColumn,
        DataColumn,
        ColumnCount
    };

    enum DbcColumn {
        DbcRealtimeColumn,
        DbcSeenColumn,
        DbcFrequencyColumn,
        DbcChannelColumn,
        DbcIdColumn,
        DbcMessageColumn,
        DbcLengthColumn,
        DbcDataColumn,
        DbcSignalsColumn,
        DbcColumnCount
    };

    QVariant dbcData(const CanFrame& frame, int column, int role) const;
    static QString formatFrequency(double hz);

    QVector<CanFrame> frames_;
    RxViewMode mode_ = RxViewMode::Fault;
    const Dbc::Database* dbc_ = nullptr;
    bool j1939Match_ = false;
    AppLanguage language_ = AppLanguage::English;
};
