#pragma once

#include "can_types.h"
#include "usb_can_service.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QTimer;
class QToolButton;

class SessionWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SessionWidget(QWidget* parent = nullptr);
    ~SessionWidget() override;

    void shutdown();

signals:
    void headerChanged(const QString& status);
    void closeRequested();

private slots:
    void refreshDevices();
    void connectDevice();
    void disconnectDevice();
    void addTxFrame();
    void removeTxFrame();
    void clearRxFrames();
    void clearLog();
    void clearRxFilters();
    void exportTxCsv();
    void importTxCsv();
    void exportRxCsv();
    void exportLogCsv();
    void onFrameReceived(const CanFrame& frame);
    void onLogEmitted(const QString& message);

private:
    enum TxColumn {
        TxActionColumn,
        TxNameColumn,
        TxExtColumn,
        TxRtrColumn,
        TxIntervalColumn,
        TxIdColumn,
        TxDataColumn,
        TxColumnCount
    };

    void buildUi();
    void applyStyle();
    void updateConnectionState();
    void appendLog(const QString& message);

    void addTxRow(const TxFrame& frame);
    TxFrame readTxRow(int row) const;
    int selectedTxRow() const;
    int rowForCycleButton(QToolButton* button) const;
    QToolButton* cycleButtonAt(int row) const;
    void manualSendRow(int row);
    void startCycleLoop(QToolButton* button);
    void stopCycleLoop(QToolButton* button);
    void stopAllCycleLoops();

    void upsertGroupedFrame(const CanFrame& frame);
    void rebuildGroupedIndex();
    QString groupedKey(const CanFrame& frame) const;
    bool frameMatchesFilters(const CanFrame& frame) const;
    void refreshRxTable();
    QVector<CanFrame> activeRxFrames() const;

    void updateWatchValue(const CanFrame& frame);
    bool tryReadWatchConfig(quint32& watchId, int& startByte, int& length, bool& littleEndian, bool& isSigned) const;

    static QString csv(const QString& value);
    static QStringList parseCsvLine(const QString& line);

    UsbCanService service_;
    QVector<CanDeviceInfo> devices_;
    QVector<CanFrame> allFrames_;
    QVector<CanFrame> groupedFrames_;
    QHash<QString, int> groupedIndex_;
    QHash<QToolButton*, QTimer*> cycleTimers_;
    int txFrameCounter_ = 0;

    QComboBox* deviceCombo_ = nullptr;
    QComboBox* channelCombo_ = nullptr;
    QComboBox* baudCombo_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QComboBox* filterCombo_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;

    QCheckBox* defaultExtCheck_ = nullptr;
    QCheckBox* defaultRtrCheck_ = nullptr;
    QTableWidget* txTable_ = nullptr;

    QLineEdit* rxFilterId_ = nullptr;
    QComboBox* rxFilterType_ = nullptr;
    QLineEdit* rxFilterPayload_ = nullptr;
    QCheckBox* groupFramesCheck_ = nullptr;
    QTableWidget* rxTable_ = nullptr;

    QWidget* watchPanel_ = nullptr;
    QLineEdit* watchId_ = nullptr;
    QLineEdit* watchStartByte_ = nullptr;
    QComboBox* watchLength_ = nullptr;
    QComboBox* watchEndian_ = nullptr;
    QCheckBox* watchSigned_ = nullptr;
    QLineEdit* watchFormula_ = nullptr;
    QLabel* watchRawValue_ = nullptr;
    QLabel* watchComputedValue_ = nullptr;

    QTextEdit* logText_ = nullptr;
};
