#pragma once

#include "can_types.h"
#include "dbc.h"
#include "dbc_signal_model.h"
#include "localization.h"
#include "nmea2000.h"
#include "rx_frame_table_model.h"
#include "usb_can_service.h"

#include <QHash>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class QTableWidget;
class QTextEdit;
class QTimer;
class QToolButton;
class QSplitter;
class QResizeEvent;

class SessionWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SessionWidget(QWidget* parent = nullptr);
    ~SessionWidget() override;

    void shutdown();
    void setLanguage(AppLanguage language);

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
    void setOperatingMode(int index);
    void loadDbc();
    void onFramesReceived(const QVector<CanFrame>& frames);
    void onRawFramesReceived(const QVector<CanFrame>& frames);
    void flushPendingRxFrames();
    void onLogEmitted(const QString& message);

private:
    static constexpr qint64 MinFrequencyElapsedMs = 1000;
    static constexpr int RxFlushIntervalMs = 100;
    static constexpr int RxCatchUpFlushDelayMs = 1;
    static constexpr int MaxFramesPerRxFlush = 750;

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

    struct RxFrameStats
    {
        int seenCount = 0;
        QString firstSeenTimestamp;
        QString lastSeenTimestamp;
        qint64 firstSeenMs = 0;
        qint64 lastSeenMs = 0;
        double receiveFrequencyHz = 0.0;
    };

    struct AdapterConfig
    {
        QString deviceKey;
        quint32 channelIndex = 0;
        QString baudRate;
        bool listenOnly = false;
        bool singleFilter = false;
    };

    void buildUi();
    void applyStyle();
    void updateAdaptiveLayout();
    void updateConnectionState();
    void updateOperatingModeUi();
    void captureTranslationKeys();
    void retranslateUi();
    void appendLog(const QString& message);
    bool loadDbcFile(const QString& path);
    void updateDbcSignals(const QVector<CanFrame>& frames);
    void updateDbcFileLabel();
    void resizeEvent(QResizeEvent* event) override;

    void addTxRow(const TxFrame& frame);
    TxFrame readTxRow(int row) const;
    int selectedTxRow() const;
    int rowForCycleButton(QToolButton* button) const;
    QToolButton* cycleButtonAt(int row) const;
    void manualSendRow(int row);
    void startTxQueueLoop();
    void stopTxQueueLoop();
    void sendNextTxQueueFrame();
    void startCycleLoop(QToolButton* button);
    void stopCycleLoop(QToolButton* button);
    void stopAllCycleLoops();
    void updateTxQueueButton();

    void upsertGroupedFrame(const CanFrame& frame);
    void rebuildGroupedIndex();
    QString groupedKey(const CanFrame& frame) const;
    CanFrame applyRxStats(const CanFrame& frame);
    CanFrame frameWithCurrentRxStats(const CanFrame& frame) const;
    static void applyRxStatsToFrame(CanFrame& frame, const RxFrameStats& stats);
    bool frameMatchesFilters(const CanFrame& frame) const;
    void refreshRxTable();
    const QVector<CanFrame>& activeRxFrames() const;
    void scheduleRxFlush(int delayMs);

    bool updateWatchValue(const CanFrame& frame);
    void refreshWatchValue();
    bool tryReadWatchConfig(quint32& watchId, int& startByte, int& length, bool& littleEndian, bool& isSigned) const;

    static QString csv(const QString& value);
    static QStringList parseCsvLine(const QString& line);
    static QString canToolDocumentsPath(const QString& fileName = QString());
    static double receiveFrequencyHz(int seenCount, qint64 firstSeenMs, qint64 lastSeenMs);
    static QString formatFrequency(double hz);

    UsbCanService service_;
    QVector<CanDeviceInfo> devices_;
    QVector<CanFrame> allFrames_;
    QVector<CanFrame> groupedFrames_;
    QVector<CanFrame> pendingRxFrames_;
    QHash<QString, int> groupedIndex_;
    QHash<QString, RxFrameStats> rxStats_;
    QHash<QToolButton*, QTimer*> cycleTimers_;
    QTimer* txQueueTimer_ = nullptr;
    int txFrameCounter_ = 0;
    int txQueueRow_ = 0;
    bool rxFlushScheduled_ = false;

    QSplitter* sectionSplitter_ = nullptr;
    QSplitter* middleSplitter_ = nullptr;

    QComboBox* deviceCombo_ = nullptr;
    QComboBox* channelCombo_ = nullptr;
    QComboBox* baudCombo_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QComboBox* filterCombo_ = nullptr;
    QComboBox* profileCombo_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QLabel* connectionStatus_ = nullptr;

    QCheckBox* defaultExtCheck_ = nullptr;
    QCheckBox* defaultRtrCheck_ = nullptr;
    QPushButton* txQueueButton_ = nullptr;
    QGroupBox* txBox_ = nullptr;
    QTableWidget* txTable_ = nullptr;

    QLineEdit* rxFilterId_ = nullptr;
    QComboBox* rxFilterType_ = nullptr;
    QLineEdit* rxFilterPayload_ = nullptr;
    QCheckBox* groupFramesCheck_ = nullptr;
    QTableView* rxTable_ = nullptr;
    RxFrameTableModel* rxModel_ = nullptr;

    QWidget* dbcBar_ = nullptr;
    QLabel* dbcFileLabel_ = nullptr;
    QCheckBox* dbcJ1939Check_ = nullptr;
    QWidget* dbcSignalsPanel_ = nullptr;
    QLineEdit* dbcSignalFilter_ = nullptr;
    QTableView* dbcSignalTable_ = nullptr;
    DbcSignalModel* dbcSignalModel_ = nullptr;

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

    bool nmeaMode_ = false;
    bool dbcMode_ = false;
    Dbc::Database dbc_;
    QString dbcPath_;
    AppLanguage language_ = AppLanguage::English;
    std::optional<AdapterConfig> savedFaultConfig_;
    Nmea2000::Decoder nmeaDecoder_;
    QVector<CanFrame> nmeaFrames_;
    QVector<CanFrame> nmeaGroupedFrames_;
    QHash<QString, int> nmeaGroupedIndex_;
};
