#pragma once

#include "can_types.h"
#include "localization.h"

#include <QAbstractTableModel>
#include <QVector>

class RxFrameTableModel final : public QAbstractTableModel
{
public:
    explicit RxFrameTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setFrames(QVector<CanFrame> frames);
    void setNmeaMode(bool enabled);
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

    static QString formatFrequency(double hz);

    QVector<CanFrame> frames_;
    bool nmeaMode_ = false;
    AppLanguage language_ = AppLanguage::English;
};
