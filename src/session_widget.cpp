#include "session_widget.h"
#include "nmea_dictionary.h"

#include "control_can_native.h"
#include "formula_evaluator.h"

#include <QAbstractButton>
#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>
#include <QStyle>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <stdexcept>

namespace
{
void relaxSplitterChild(QWidget* widget)
{
    widget->setMinimumSize(0, 0);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void relaxTable(QTableWidget* table)
{
    relaxSplitterChild(table);
    table->horizontalHeader()->setMinimumSectionSize(1);
    table->verticalHeader()->setMinimumSectionSize(1);
}

void relaxTableView(QTableView* table)
{
    relaxSplitterChild(table);
    table->horizontalHeader()->setMinimumSectionSize(1);
    table->verticalHeader()->setMinimumSectionSize(1);
}
}

SessionWidget::SessionWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyStyle();

    connect(&service_, &UsbCanService::framesReceived, this, &SessionWidget::onFramesReceived);
    connect(&service_, &UsbCanService::rawFramesReceived, this, &SessionWidget::onRawFramesReceived);
    connect(&service_, &UsbCanService::logEmitted, this, &SessionWidget::onLogEmitted);
    connect(&service_, &UsbCanService::connectionLost, this, &SessionWidget::disconnectDevice);

    refreshDevices();
    updateDbcFileLabel();
    updateConnectionState();
}

SessionWidget::~SessionWidget()
{
    shutdown();
}

void SessionWidget::shutdown()
{
    stopTxQueueLoop();
    stopAllCycleLoops();
    service_.disconnectDevice();
}

void SessionWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 0, 12, 12);
    root->setSpacing(12);

    sectionSplitter_ = new QSplitter(Qt::Vertical, this);
    sectionSplitter_->setObjectName("sectionSplitter");
    sectionSplitter_->setChildrenCollapsible(true);
    sectionSplitter_->setOpaqueResize(true);
    sectionSplitter_->setHandleWidth(5);
    root->addWidget(sectionSplitter_, 1);

    auto* header = new QGroupBox(this);
    header->setObjectName("headerPanel");
    relaxSplitterChild(header);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(14, 12, 14, 12);
    headerLayout->setSpacing(8);
    headerLayout->setSizeConstraint(QLayout::SetNoConstraint);

    deviceCombo_ = new QComboBox(header);
    channelCombo_ = new QComboBox(header);
    baudCombo_ = new QComboBox(header);
    modeCombo_ = new QComboBox(header);
    filterCombo_ = new QComboBox(header);
    profileCombo_ = new QComboBox(header);
    connectButton_ = new QPushButton("Connect", header);
    disconnectButton_ = new QPushButton("Disconnect", header);
    auto* refreshButton = new QPushButton("Refresh", header);
    auto* closeButton = new QPushButton("Close Session", header);
    connectionStatus_ = new QLabel("DISCONNECTED", header);
    connectionStatus_->setObjectName("connectionStatus");
    connectionStatus_->setAlignment(Qt::AlignCenter);
    connectionStatus_->setProperty("state", "inactive");
    connectButton_->setObjectName("primaryButton");
    disconnectButton_->setObjectName("warningButton");
    refreshButton->setObjectName("secondaryButton");
    closeButton->setObjectName("dangerButton");
    connectButton_->setProperty("variant", "primary");
    disconnectButton_->setProperty("tone", "warn");
    closeButton->setProperty("tone", "danger");

    channelCombo_->addItems({"0", "1"});
    baudCombo_->addItems({"1M", "500K", "250K", "125K"});
    baudCombo_->setCurrentText("500K");
    modeCombo_->addItem("Normal", "Normal");
    modeCombo_->addItem("Listen Only", "Listen Only");
    filterCombo_->addItem("Accept All", "Accept All");
    filterCombo_->addItem("Single Filter", "Single Filter");
    profileCombo_->addItems({"Fault Reading", "NMEA 2000", "DBC"});
    profileCombo_->setToolTip("Select the adapter operating profile. NMEA 2000 is receive-only.");

    auto* identityRow = new QHBoxLayout();
    identityRow->setContentsMargins(0, 0, 0, 0);
    auto* identityText = new QVBoxLayout();
    identityText->setContentsMargins(0, 0, 0, 0);
    identityText->setSpacing(1);
    auto* familyLabel = new QLabel("ZVRNK TOOL FAMILY / CAN BUS TOOL", header);
    familyLabel->setObjectName("eyebrowLabel");
    auto* appLabel = new QLabel("ZVRNK CAN Tool", header);
    appLabel->setObjectName("titleLabel");
    auto* subtitleLabel = new QLabel("Configure, transmit, and inspect live CAN traffic.", header);
    subtitleLabel->setObjectName("subtitleLabel");
    identityText->addWidget(familyLabel);
    identityText->addWidget(appLabel);
    identityText->addWidget(subtitleLabel);
    identityRow->addLayout(identityText, 1);
    identityRow->addWidget(connectionStatus_, 0, Qt::AlignVCenter);
    headerLayout->addLayout(identityRow);

    auto* connectionGrid = new QGridLayout();
    connectionGrid->setContentsMargins(0, 0, 0, 0);
    connectionGrid->setHorizontalSpacing(8);
    connectionGrid->setVerticalSpacing(5);
    const auto addLabeled = [connectionGrid](const QString& label, QWidget* widget, int column) {
        auto* text = new QLabel(label);
        connectionGrid->addWidget(text, 0, column);
        connectionGrid->addWidget(widget, 1, column);
    };
    addLabeled("Profile", profileCombo_, 0);
    addLabeled("Device", deviceCombo_, 1);
    addLabeled("Channel", channelCombo_, 2);
    addLabeled("Baud", baudCombo_, 3);
    addLabeled("Mode", modeCombo_, 4);
    addLabeled("Filter", filterCombo_, 5);
    connectionGrid->addWidget(refreshButton, 2, 0);
    connectionGrid->addWidget(connectButton_, 2, 1);
    connectionGrid->addWidget(disconnectButton_, 2, 2);
    connectionGrid->addWidget(closeButton, 2, 3);
    for (int column = 0; column < 6; ++column) {
        connectionGrid->setColumnStretch(column, 1);
    }
    headerLayout->addLayout(connectionGrid);
    sectionSplitter_->addWidget(header);

    connect(refreshButton, &QPushButton::clicked, this, &SessionWidget::refreshDevices);
    connect(connectButton_, &QPushButton::clicked, this, &SessionWidget::connectDevice);
    connect(disconnectButton_, &QPushButton::clicked, this, &SessionWidget::disconnectDevice);
    connect(closeButton, &QPushButton::clicked, this, &SessionWidget::closeRequested);
    connect(profileCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &SessionWidget::setOperatingMode);
    connect(modeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        if (!nmeaMode_) {
            return;
        }
        const bool wasConnected = service_.isConnected();
        if (wasConnected) {
            service_.disconnectDevice();
            emit headerChanged("Disconnected");
            connectDevice();
        }
        updateConnectionState();
    });

    middleSplitter_ = new QSplitter(Qt::Horizontal, this);
    middleSplitter_->setObjectName("panelSplitter");
    middleSplitter_->setChildrenCollapsible(true);
    middleSplitter_->setOpaqueResize(true);
    middleSplitter_->setHandleWidth(5);
    sectionSplitter_->addWidget(middleSplitter_);

    txBox_ = new QGroupBox("Transmit", this);
    relaxSplitterChild(txBox_);
    auto* txLayout = new QVBoxLayout(txBox_);
    txLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* txActions = new QGridLayout();
    auto* addButton = new QPushButton("Add", txBox_);
    auto* removeButton = new QPushButton("Remove", txBox_);
    txQueueButton_ = new QPushButton("Loop Queue", txBox_);
    auto* exportTxButton = new QPushButton("Export CSV", txBox_);
    auto* importTxButton = new QPushButton("Import CSV", txBox_);
    addButton->setObjectName("primaryButton");
    removeButton->setObjectName("dangerButton");
    txQueueButton_->setObjectName("toggleButton");
    txQueueButton_->setCheckable(true);
    txQueueButton_->setToolTip("Send one transmit row every 200 ms, looping from first to last");
    exportTxButton->setObjectName("secondaryButton");
    importTxButton->setObjectName("secondaryButton");
    addButton->setProperty("variant", "primary");
    removeButton->setProperty("tone", "danger");
    defaultExtCheck_ = new QCheckBox("Extended Frame", txBox_);
    defaultRtrCheck_ = new QCheckBox("Remote Frame", txBox_);
    defaultExtCheck_->setChecked(true);
    txActions->addWidget(defaultExtCheck_, 0, 0);
    txActions->addWidget(defaultRtrCheck_, 0, 1);
    txActions->addWidget(addButton, 0, 2);
    txActions->addWidget(removeButton, 0, 3);
    txActions->addWidget(txQueueButton_, 1, 0);
    txActions->addWidget(exportTxButton, 1, 1);
    txActions->addWidget(importTxButton, 1, 2);
    txActions->setColumnStretch(4, 1);
    txLayout->addLayout(txActions);

    txTable_ = new QTableWidget(0, TxColumnCount, txBox_);
    relaxTable(txTable_);
    txTable_->setHorizontalHeaderLabels({"Actions", "Name", "Ext", "RTR", "Interval(ms)", "ID", "Data"});
    txTable_->horizontalHeader()->setSectionResizeMode(TxActionColumn, QHeaderView::ResizeToContents);
    txTable_->horizontalHeader()->setSectionResizeMode(TxDataColumn, QHeaderView::Stretch);
    txTable_->verticalHeader()->setVisible(false);
    txTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    txTable_->verticalHeader()->setDefaultSectionSize(34);
    txTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    txTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    txTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    txTable_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    txLayout->addWidget(txTable_, 1);
    middleSplitter_->addWidget(txBox_);

    connect(addButton, &QPushButton::clicked, this, &SessionWidget::addTxFrame);
    connect(removeButton, &QPushButton::clicked, this, &SessionWidget::removeTxFrame);
    connect(txQueueButton_, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            startTxQueueLoop();
        } else {
            stopTxQueueLoop();
        }
    });
    connect(exportTxButton, &QPushButton::clicked, this, &SessionWidget::exportTxCsv);
    connect(importTxButton, &QPushButton::clicked, this, &SessionWidget::importTxCsv);

    auto* rxBox = new QGroupBox("Received Frames", this);
    rxBox->setObjectName("receivedFramesPanel");
    relaxSplitterChild(rxBox);
    auto* rxLayout = new QVBoxLayout(rxBox);
    rxLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* rxTop = new QHBoxLayout();
    auto* exportRxButton = new QPushButton("Export CSV", rxBox);
    auto* watchButton = new QPushButton("Watch", rxBox);
    watchButton->setCheckable(true);
    auto* clearRxButton = new QPushButton("Clear List", rxBox);
    exportRxButton->setObjectName("secondaryButton");
    watchButton->setObjectName("toggleButton");
    clearRxButton->setObjectName("dangerButton");
    clearRxButton->setProperty("tone", "danger");
    rxTop->addStretch();
    rxTop->addWidget(exportRxButton);
    rxTop->addWidget(watchButton);
    rxTop->addWidget(clearRxButton);
    rxLayout->addLayout(rxTop);

    dbcBar_ = new QWidget(rxBox);
    auto* dbcLayout = new QHBoxLayout(dbcBar_);
    dbcLayout->setContentsMargins(0, 0, 0, 0);
    auto* loadDbcButton = new QPushButton("Load DBC...", dbcBar_);
    loadDbcButton->setObjectName("primaryButton");
    loadDbcButton->setProperty("variant", "primary");
    dbcFileLabel_ = new QLabel(dbcBar_);
    dbcFileLabel_->setProperty("dynamicText", true);
    dbcFileLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    dbcJ1939Check_ = new QCheckBox("J1939 match", dbcBar_);
    dbcJ1939Check_->setToolTip("Match extended IDs while ignoring the J1939 source address");
    dbcJ1939Check_->setChecked(QSettings().value("dbc/j1939Match", false).toBool());
    dbcLayout->addWidget(loadDbcButton);
    dbcLayout->addWidget(dbcFileLabel_, 1);
    dbcLayout->addWidget(dbcJ1939Check_);
    dbcBar_->setVisible(false);
    rxLayout->addWidget(dbcBar_);
    connect(loadDbcButton, &QPushButton::clicked, this, &SessionWidget::loadDbc);

    auto* filters = new QGridLayout();
    rxFilterId_ = new QLineEdit(rxBox);
    rxFilterType_ = new QComboBox(rxBox);
    rxFilterPayload_ = new QLineEdit(rxBox);
    groupFramesCheck_ = new QCheckBox("Group Unique", rxBox);
    groupFramesCheck_->setChecked(true);
    auto* resetFilterButton = new QPushButton("Reset", rxBox);
    resetFilterButton->setObjectName("secondaryButton");
    rxFilterType_->addItem("ALL", "ALL");
    rxFilterType_->addItem("STD", "STD");
    rxFilterType_->addItem("EXT", "EXT");
    rxFilterType_->addItem("RTR", "RTR");
    rxFilterId_->setPlaceholderText("Filter ID");
    rxFilterPayload_->setPlaceholderText("Payload Contains");
    filters->addWidget(rxFilterId_, 0, 0);
    filters->addWidget(rxFilterType_, 0, 1);
    filters->addWidget(rxFilterPayload_, 0, 2);
    filters->addWidget(groupFramesCheck_, 1, 0, 1, 2);
    filters->addWidget(resetFilterButton, 1, 2);
    filters->setColumnStretch(0, 2);
    filters->setColumnStretch(1, 1);
    filters->setColumnStretch(2, 3);
    rxLayout->addLayout(filters);

    watchPanel_ = new QWidget(rxBox);
    watchPanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* watchLayout = new QGridLayout(watchPanel_);
    watchLayout->setSizeConstraint(QLayout::SetNoConstraint);
    watchId_ = new QLineEdit("0x18FF0080", watchPanel_);
    watchStartByte_ = new QLineEdit("0", watchPanel_);
    watchLength_ = new QComboBox(watchPanel_);
    watchEndian_ = new QComboBox(watchPanel_);
    watchSigned_ = new QCheckBox("Signed", watchPanel_);
    watchFormula_ = new QLineEdit("x", watchPanel_);
    watchRawValue_ = new QLabel("-", watchPanel_);
    watchComputedValue_ = new QLabel("-", watchPanel_);
    watchLength_->addItems({"1", "2", "3", "4"});
    watchEndian_->addItem("Little", "Little");
    watchEndian_->addItem("Big", "Big");
    watchLayout->addWidget(new QLabel("Watch ID"), 0, 0);
    watchLayout->addWidget(new QLabel("Start Byte"), 0, 1);
    watchLayout->addWidget(new QLabel("Length"), 0, 2);
    watchLayout->addWidget(new QLabel("Endian"), 0, 3);
    watchLayout->addWidget(watchId_, 1, 0);
    watchLayout->addWidget(watchStartByte_, 1, 1);
    watchLayout->addWidget(watchLength_, 1, 2);
    watchLayout->addWidget(watchEndian_, 1, 3);
    watchLayout->addWidget(new QLabel("Formula (x = dec)"), 2, 0);
    watchLayout->addWidget(watchSigned_, 2, 1);
    watchLayout->addWidget(new QLabel("Raw Value (dec)"), 2, 2);
    watchLayout->addWidget(new QLabel("Computed Value"), 2, 3);
    watchLayout->addWidget(watchFormula_, 3, 0, 1, 2);
    watchLayout->addWidget(watchRawValue_, 3, 2);
    watchLayout->addWidget(watchComputedValue_, 3, 3);
    watchLayout->setColumnStretch(0, 2);
    watchLayout->setColumnStretch(1, 2);
    watchLayout->setColumnStretch(2, 1);
    watchLayout->setColumnStretch(3, 1);
    watchPanel_->setMinimumHeight(116);
    watchPanel_->setVisible(false);
    rxLayout->addWidget(watchPanel_);

    rxModel_ = new RxFrameTableModel(this);
    rxTable_ = new QTableView(rxBox);
    relaxTableView(rxTable_);
    rxTable_->setModel(rxModel_);
    rxTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    rxTable_->verticalHeader()->setVisible(false);
    rxTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rxTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rxTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    rxTable_->setAlternatingRowColors(true);
    rxTable_->setWordWrap(false);
    rxTable_->setShowGrid(false);
    rxTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rxTable_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    rxTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    rxTable_->verticalHeader()->setDefaultSectionSize(24);

    dbcSignalsPanel_ = new QWidget(rxBox);
    relaxSplitterChild(dbcSignalsPanel_);
    auto* signalsLayout = new QVBoxLayout(dbcSignalsPanel_);
    signalsLayout->setContentsMargins(0, 0, 0, 0);
    auto* signalsTitle = new QLabel("Signals", dbcSignalsPanel_);
    dbcSignalFilter_ = new QLineEdit(dbcSignalsPanel_);
    dbcSignalFilter_->setPlaceholderText("Filter signals");
    auto* signalsHeader = new QHBoxLayout();
    signalsHeader->addWidget(signalsTitle);
    signalsHeader->addWidget(dbcSignalFilter_, 1);
    signalsLayout->addLayout(signalsHeader);
    dbcSignalModel_ = new DbcSignalModel(this);
    dbcSignalModel_->setDatabase(&dbc_);
    dbcSignalTable_ = new QTableView(dbcSignalsPanel_);
    relaxTableView(dbcSignalTable_);
    dbcSignalTable_->setModel(dbcSignalModel_);
    dbcSignalTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    dbcSignalTable_->verticalHeader()->setVisible(false);
    dbcSignalTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    dbcSignalTable_->verticalHeader()->setDefaultSectionSize(24);
    dbcSignalTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dbcSignalTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    dbcSignalTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    dbcSignalTable_->setAlternatingRowColors(true);
    dbcSignalTable_->setShowGrid(false);
    dbcSignalTable_->setWordWrap(false);
    dbcSignalTable_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    dbcSignalTable_->setAccessibleName("DBC signal values");
    signalsLayout->addWidget(dbcSignalTable_, 1);
    dbcSignalsPanel_->setVisible(false);
    connect(dbcSignalFilter_, &QLineEdit::textChanged, this, [this](const QString& text) {
        dbcSignalModel_->setFilter(text);
    });
    connect(dbcJ1939Check_, &QCheckBox::toggled, this, [this](bool checked) {
        rxModel_->setDbc(&dbc_, checked);
        dbcSignalModel_->clear();
        QSettings().setValue("dbc/j1939Match", checked);
        refreshRxTable();
    });

    auto* rxSplitter = new QSplitter(Qt::Vertical, rxBox);
    rxSplitter->setChildrenCollapsible(true);
    rxSplitter->setOpaqueResize(true);
    rxSplitter->setHandleWidth(5);
    rxSplitter->addWidget(rxTable_);
    rxSplitter->addWidget(dbcSignalsPanel_);
    rxSplitter->setStretchFactor(0, 3);
    rxSplitter->setStretchFactor(1, 2);
    rxLayout->addWidget(rxSplitter, 1);
    middleSplitter_->addWidget(rxBox);
    middleSplitter_->setStretchFactor(0, 4);
    middleSplitter_->setStretchFactor(1, 6);

    connect(exportRxButton, &QPushButton::clicked, this, &SessionWidget::exportRxCsv);
    connect(watchButton, &QPushButton::toggled, this, [this](bool visible) {
        watchPanel_->setVisible(visible);
        if (visible) {
            refreshWatchValue();
        }
    });
    connect(clearRxButton, &QPushButton::clicked, this, &SessionWidget::clearRxFrames);
    connect(resetFilterButton, &QPushButton::clicked, this, &SessionWidget::clearRxFilters);
    connect(rxFilterId_, &QLineEdit::textChanged, this, &SessionWidget::refreshRxTable);
    connect(rxFilterPayload_, &QLineEdit::textChanged, this, &SessionWidget::refreshRxTable);
    connect(rxFilterType_, &QComboBox::currentTextChanged, this, &SessionWidget::refreshRxTable);
    connect(groupFramesCheck_, &QCheckBox::toggled, this, &SessionWidget::refreshRxTable);
    connect(watchId_, &QLineEdit::textChanged, this, &SessionWidget::refreshWatchValue);
    connect(watchStartByte_, &QLineEdit::textChanged, this, &SessionWidget::refreshWatchValue);
    connect(watchLength_, &QComboBox::currentTextChanged, this, &SessionWidget::refreshWatchValue);
    connect(watchEndian_, &QComboBox::currentTextChanged, this, &SessionWidget::refreshWatchValue);
    connect(watchSigned_, &QCheckBox::toggled, this, &SessionWidget::refreshWatchValue);
    connect(watchFormula_, &QLineEdit::textChanged, this, &SessionWidget::refreshWatchValue);

    auto* logBox = new QGroupBox("Event Log", this);
    relaxSplitterChild(logBox);
    auto* logLayout = new QVBoxLayout(logBox);
    logLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* logActions = new QHBoxLayout();
    auto* exportLogButton = new QPushButton("Export CSV", logBox);
    auto* clearLogButton = new QPushButton("Clear Log", logBox);
    exportLogButton->setObjectName("secondaryButton");
    clearLogButton->setObjectName("dangerButton");
    clearLogButton->setProperty("tone", "danger");
    logActions->addStretch();
    logActions->addWidget(exportLogButton);
    logActions->addWidget(clearLogButton);
    logLayout->addLayout(logActions);
    logText_ = new QTextEdit(logBox);
    relaxSplitterChild(logText_);
    logText_->setReadOnly(true);
    logText_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    logLayout->addWidget(logText_);
    sectionSplitter_->addWidget(logBox);
    sectionSplitter_->setStretchFactor(0, 0);
    sectionSplitter_->setStretchFactor(1, 6);
    sectionSplitter_->setStretchFactor(2, 2);

    txTable_->setAccessibleName("Transmit frame table");
    rxTable_->setAccessibleName("Received CAN frame table");
    logText_->setAccessibleName("Event log");
    captureTranslationKeys();
    updateAdaptiveLayout();

    connect(exportLogButton, &QPushButton::clicked, this, &SessionWidget::exportLogCsv);
    connect(clearLogButton, &QPushButton::clicked, this, &SessionWidget::clearLog);
}

void SessionWidget::applyStyle()
{
    // The application-level theme is owned by MainWindow so all sessions stay consistent.
}

void SessionWidget::setLanguage(AppLanguage language)
{
    language_ = language;
    captureTranslationKeys();
    retranslateUi();
    updateConnectionState();
    updateOperatingModeUi();
    updateTxQueueButton();
}

void SessionWidget::captureTranslationKeys()
{
    const auto captureWidget = [](QWidget* widget, const QString& text) {
        if (widget == nullptr || text.isEmpty() || widget->property("dynamicText").toBool()) {
            return;
        }
        if (!widget->property("i18nSource").isValid()) {
            widget->setProperty("i18nSource", text);
        }
    };

    for (QLabel* label : findChildren<QLabel*>()) {
        captureWidget(label, label->text());
    }
    for (QAbstractButton* button : findChildren<QAbstractButton*>()) {
        captureWidget(button, button->text());
    }
    for (QGroupBox* groupBox : findChildren<QGroupBox*>()) {
        captureWidget(groupBox, groupBox->title());
    }
    for (QComboBox* combo : findChildren<QComboBox*>()) {
        if (!combo->property("i18nItems").isValid() && combo->count() > 0) {
            QStringList items;
            for (int index = 0; index < combo->count(); ++index) {
                items.push_back(combo->itemText(index));
            }
            combo->setProperty("i18nItems", items);
        }
    }
    for (QLineEdit* edit : findChildren<QLineEdit*>()) {
        if (!edit->property("i18nPlaceholder").isValid() && !edit->placeholderText().isEmpty()) {
            edit->setProperty("i18nPlaceholder", edit->placeholderText());
        }
    }
    for (QWidget* widget : findChildren<QWidget*>()) {
        if (!widget->toolTip().isEmpty() && !widget->property("i18nToolTip").isValid()) {
            widget->setProperty("i18nToolTip", widget->toolTip());
        }
        if (!widget->accessibleName().isEmpty() && !widget->property("i18nAccessibleName").isValid()) {
            widget->setProperty("i18nAccessibleName", widget->accessibleName());
        }
    }
}

void SessionWidget::retranslateUi()
{
    const auto translated = [this](const QString& source) {
        return localize(language_, source);
    };

    for (QLabel* label : findChildren<QLabel*>()) {
        const QVariant source = label->property("i18nSource");
        if (source.isValid()) {
            label->setText(translated(source.toString()));
        }
    }
    for (QAbstractButton* button : findChildren<QAbstractButton*>()) {
        const QVariant source = button->property("i18nSource");
        if (source.isValid()) {
            button->setText(translated(source.toString()));
        }
    }
    for (QToolButton* button : findChildren<QToolButton*>()) {
        if (button->property("cycleControl").toBool()) {
            button->setText(translated(button->isChecked() ? "Stop" : "Cycle"));
        }
    }
    for (QGroupBox* groupBox : findChildren<QGroupBox*>()) {
        const QVariant source = groupBox->property("i18nSource");
        if (source.isValid()) {
            groupBox->setTitle(translated(source.toString()));
        }
    }
    for (QComboBox* combo : findChildren<QComboBox*>()) {
        const QStringList sources = combo->property("i18nItems").toStringList();
        for (int index = 0; index < std::min(combo->count(), static_cast<int>(sources.size())); ++index) {
            combo->setItemText(index, translated(sources[index]));
        }
    }
    for (QLineEdit* edit : findChildren<QLineEdit*>()) {
        const QVariant source = edit->property("i18nPlaceholder");
        if (source.isValid()) {
            edit->setPlaceholderText(translated(source.toString()));
        }
    }
    for (QWidget* widget : findChildren<QWidget*>()) {
        const QVariant toolTip = widget->property("i18nToolTip");
        if (toolTip.isValid()) {
            widget->setToolTip(translated(toolTip.toString()));
        }
        const QVariant accessibleName = widget->property("i18nAccessibleName");
        if (accessibleName.isValid()) {
            widget->setAccessibleName(translated(accessibleName.toString()));
        }
    }
    rxModel_->setLanguage(language_);
    dbcSignalModel_->setLanguage(language_);
    updateDbcFileLabel();
}

void SessionWidget::updateAdaptiveLayout()
{
    if (!middleSplitter_) {
        return;
    }

    const bool compact = width() < 1040;
    const Qt::Orientation wanted = compact ? Qt::Vertical : Qt::Horizontal;
    if (middleSplitter_->orientation() != wanted) {
        middleSplitter_->setOrientation(wanted);
    }

    if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
        const int margin = compact ? 8 : 12;
        root->setContentsMargins(margin, 0, margin, margin);
        root->setSpacing(compact ? 8 : 12);
    }
}

void SessionWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateAdaptiveLayout();
}

void SessionWidget::refreshDevices()
{
    QString controlCanError;
    devices_ = UsbCanService::findDevices(&controlCanError);
    deviceCombo_->clear();
    for (const auto& device : devices_) {
        deviceCombo_->addItem(device.displayName(), device.key());
    }
    if (!controlCanError.isEmpty()) {
        appendLog(QString("ControlCAN adapters unavailable: %1").arg(controlCanError));
    }
    const auto serialCount = std::count_if(devices_.cbegin(), devices_.cend(), [](const CanDeviceInfo& device) {
        return device.backend == CanBackend::Gvret;
    });
    appendLog(QString("Device list refreshed. Found %1 ControlCAN device(s) and %2 serial port(s) for GVRET.")
                  .arg(devices_.size() - serialCount)
                  .arg(serialCount));
}

void SessionWidget::connectDevice()
{
    try {
        if (deviceCombo_->currentIndex() < 0) {
            throw std::runtime_error("Select a device first.");
        }
        const QString deviceKey = deviceCombo_->currentData().toString();
        const auto device = std::find_if(devices_.cbegin(), devices_.cend(), [&deviceKey](const CanDeviceInfo& candidate) {
            return candidate.key() == deviceKey;
        });
        if (device == devices_.cend()) {
            throw std::runtime_error("Selected device is no longer listed. Refresh the device list.");
        }
        const quint32 channelIndex = channelCombo_->currentText().toUInt();
        const QString baudRate = nmeaMode_ ? QStringLiteral("250K") : baudCombo_->currentText();
        const bool listenOnly = nmeaMode_ || modeCombo_->currentData().toString() == "Listen Only";
        const bool singleFilter = !nmeaMode_ && filterCombo_->currentData().toString() == "Single Filter";

        service_.setNmeaMonitorMode(nmeaMode_);
        service_.connectDevice(*device, channelIndex, baudRate, listenOnly, singleFilter);
        emit headerChanged(QString("%1 / CH %2").arg(device->shortName()).arg(channelIndex));
        updateConnectionState();
    } catch (const std::exception& ex) {
        appendLog(QString("Connect failed: %1").arg(ex.what()));
        QMessageBox::critical(this, "Connect Error", ex.what());
    }
}

void SessionWidget::setOperatingMode(int index)
{
    const bool requestedNmeaMode = index == 1;
    const bool requestedDbcMode = index == 2;
    if (requestedNmeaMode == nmeaMode_ && requestedDbcMode == dbcMode_) {
        updateOperatingModeUi();
        return;
    }

    // Fault reading and DBC share adapter settings, so only entering or
    // leaving NMEA 2000 needs a reconnect.
    const bool nmeaChanged = requestedNmeaMode != nmeaMode_;
    const bool wasConnected = service_.isConnected();
    if (nmeaChanged && requestedNmeaMode) {
        AdapterConfig config;
        config.deviceKey = deviceCombo_->currentData().toString();
        config.channelIndex = channelCombo_->currentText().toUInt();
        config.baudRate = baudCombo_->currentText();
        config.listenOnly = modeCombo_->currentData().toString() == "Listen Only";
        config.singleFilter = filterCombo_->currentData().toString() == "Single Filter";
        savedFaultConfig_ = config;

        if (wasConnected) {
            service_.disconnectDevice();
            emit headerChanged("Disconnected");
        }

        nmeaMode_ = true;
        nmeaDecoder_.reset();
        nmeaFrames_.clear();
        nmeaGroupedFrames_.clear();
        nmeaGroupedIndex_.clear();
        pendingRxFrames_.clear();
        stopTxQueueLoop();
        stopAllCycleLoops();
        {
            const QSignalBlocker baudBlocker(baudCombo_);
            const QSignalBlocker modeBlocker(modeCombo_);
            const QSignalBlocker filterBlocker(filterCombo_);
            baudCombo_->setCurrentText("250K");
            // Normal controller mode is still receive-only at the application
            // layer, but it emits CAN ACK bits. Some NMEA senders will not
            // publish engine PGNs when every receiver is listen-only.
            modeCombo_->setCurrentIndex(modeCombo_->findData("Normal"));
            filterCombo_->setCurrentIndex(filterCombo_->findData("Accept All"));
        }
        service_.setNmeaMonitorMode(true);
    } else if (nmeaChanged) {
        if (wasConnected) {
            service_.disconnectDevice();
            emit headerChanged("Disconnected");
        }

        nmeaMode_ = false;
        service_.setNmeaMonitorMode(false);
        nmeaDecoder_.reset();
        nmeaFrames_.clear();
        nmeaGroupedFrames_.clear();
        nmeaGroupedIndex_.clear();
        pendingRxFrames_.clear();

        if (savedFaultConfig_) {
            const AdapterConfig config = *savedFaultConfig_;
            const int deviceIndex = deviceCombo_->findData(config.deviceKey);
            const QSignalBlocker deviceBlocker(deviceCombo_);
            const QSignalBlocker channelBlocker(channelCombo_);
            const QSignalBlocker baudBlocker(baudCombo_);
            const QSignalBlocker modeBlocker(modeCombo_);
            const QSignalBlocker filterBlocker(filterCombo_);
            if (deviceIndex >= 0) {
                deviceCombo_->setCurrentIndex(deviceIndex);
            }
            channelCombo_->setCurrentText(QString::number(config.channelIndex));
            baudCombo_->setCurrentText(config.baudRate);
            modeCombo_->setCurrentIndex(modeCombo_->findData(config.listenOnly ? "Listen Only" : "Normal"));
            filterCombo_->setCurrentIndex(filterCombo_->findData(config.singleFilter ? "Single Filter" : "Accept All"));
        }
        savedFaultConfig_.reset();
    }

    dbcMode_ = requestedDbcMode;
    if (dbcMode_ && dbc_.isEmpty()) {
        const QString lastPath = QSettings().value("dbc/lastPath").toString();
        if (lastPath.isEmpty() || !QFileInfo::exists(lastPath) || !loadDbcFile(lastPath)) {
            loadDbc();
        }
    }

    updateOperatingModeUi();
    updateConnectionState();
    refreshRxTable();

    if (nmeaChanged && wasConnected) {
        connectDevice();
    }
}

void SessionWidget::loadDbc()
{
    QString startPath = dbcPath_;
    if (startPath.isEmpty()) {
        startPath = QSettings().value("dbc/lastPath", canToolDocumentsPath()).toString();
    }
    const QString path = QFileDialog::getOpenFileName(this, "Load DBC", startPath, "DBC files (*.dbc);;All files (*.*)");
    if (!path.isEmpty()) {
        loadDbcFile(path);
    }
}

bool SessionWidget::loadDbcFile(const QString& path)
{
    Dbc::Database database;
    QString error;
    if (!database.loadFile(path, &error)) {
        appendLog(QString("DBC load failed (%1): %2").arg(QFileInfo(path).fileName(), error));
        QMessageBox::warning(this, "DBC Load Error", QString("%1\n\n%2").arg(path, error));
        return false;
    }

    // Detach the views before the database they point at is replaced.
    rxModel_->setDbc(nullptr, false);
    dbcSignalModel_->setDatabase(nullptr);
    dbc_ = std::move(database);
    dbcPath_ = path;
    rxModel_->setDbc(&dbc_, dbcJ1939Check_->isChecked());
    dbcSignalModel_->setDatabase(&dbc_);
    QSettings().setValue("dbc/lastPath", path);

    // Rebuild signal values from frames already on screen.
    QVector<CanFrame> existing(groupedFrames_.crbegin(), groupedFrames_.crend());
    updateDbcSignals(existing);

    updateDbcFileLabel();
    appendLog(QString("DBC loaded: %1 (%2 messages, %3 signals).")
                  .arg(QFileInfo(path).fileName())
                  .arg(dbc_.messages().size())
                  .arg(dbc_.signalCount()));
    refreshRxTable();
    return true;
}

void SessionWidget::updateDbcSignals(const QVector<CanFrame>& frames)
{
    if (dbc_.isEmpty()) {
        return;
    }

    const bool j1939Match = dbcJ1939Check_->isChecked();
    for (const CanFrame& frame : frames) {
        const int message = dbc_.findMessage(frame.rawId, frame.frameType == "EXT", j1939Match);
        if (message < 0) {
            continue;
        }
        dbcSignalModel_->update(message, Dbc::decode(dbc_.messages()[message], frame.dataBytes), frame.timestamp, frame.seenCount);
    }
    dbcSignalModel_->publish();
}

void SessionWidget::updateDbcFileLabel()
{
    if (!dbcFileLabel_) {
        return;
    }
    if (dbc_.isEmpty()) {
        dbcFileLabel_->setText(localize(language_, "No DBC loaded"));
        dbcFileLabel_->setToolTip({});
        return;
    }
    dbcFileLabel_->setText(QString("%1  [%2 msg / %3 sig]")
                               .arg(QFileInfo(dbcPath_).fileName())
                               .arg(dbc_.messages().size())
                               .arg(dbc_.signalCount()));
    dbcFileLabel_->setToolTip(QString("%1\n%2 messages, %3 signals").arg(dbcPath_).arg(dbc_.messages().size()).arg(dbc_.signalCount()));
}

void SessionWidget::disconnectDevice()
{
    service_.disconnectDevice();
    stopTxQueueLoop();
    stopAllCycleLoops();
    emit headerChanged("Disconnected");
    updateConnectionState();
}

void SessionWidget::addTxFrame()
{
    ++txFrameCounter_;
    TxFrame frame;
    frame.name = QString("F%1").arg(txFrameCounter_);
    frame.extendedFrame = defaultExtCheck_->isChecked();
    frame.remoteFrame = defaultRtrCheck_->isChecked();
    frame.idText = frame.extendedFrame ? "0x00000000" : "0x000";
    addTxRow(frame);
    updateTxQueueButton();
}

void SessionWidget::removeTxFrame()
{
    const int row = selectedTxRow();
    if (row < 0) {
        return;
    }
    if (auto* button = cycleButtonAt(row)) {
        stopCycleLoop(button);
    }
    txTable_->removeRow(row);
    updateTxQueueButton();
}

void SessionWidget::clearRxFrames()
{
    allFrames_.clear();
    groupedFrames_.clear();
    nmeaFrames_.clear();
    nmeaGroupedFrames_.clear();
    nmeaGroupedIndex_.clear();
    pendingRxFrames_.clear();
    groupedIndex_.clear();
    rxStats_.clear();
    nmeaDecoder_.reset();
    dbcSignalModel_->clear();
    refreshRxTable();
}

void SessionWidget::clearLog()
{
    logText_->clear();
}

void SessionWidget::clearRxFilters()
{
    rxFilterId_->clear();
    rxFilterPayload_->clear();
    rxFilterType_->setCurrentIndex(rxFilterType_->findData("ALL"));
    refreshRxTable();
}

void SessionWidget::exportTxCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Transmit Frames", canToolDocumentsPath("tx_frames.csv"), "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(QString("Export TX CSV failed: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "Name,ID,Extended,Remote,CycleEnabled,Length,Data,IntervalMs\n";
    for (int row = 0; row < txTable_->rowCount(); ++row) {
        const TxFrame frame = readTxRow(row);
        int length = 0;
        try {
            length = UsbCanService::parsePayload(frame.payloadText).size();
        } catch (...) {
        }
        out << csv(frame.name) << ','
            << csv(frame.idText) << ','
            << (frame.extendedFrame ? "1" : "0") << ','
            << (frame.remoteFrame ? "1" : "0") << ','
            << (frame.cycleEnabled ? "1" : "0") << ','
            << length << ','
            << csv(frame.payloadText) << ','
            << frame.intervalMs << '\n';
    }
    appendLog(QString("TX CSV exported: %1").arg(path));
}

void SessionWidget::importTxCsv()
{
    const QString path = QFileDialog::getOpenFileName(this, "Import Transmit Frames", canToolDocumentsPath(), "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog(QString("Import TX CSV failed: %1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    const QStringList lines = in.readAll().split(QRegularExpression("\\r?\\n"), Qt::SkipEmptyParts);
    int imported = 0;
    int skipped = 0;
    for (int i = 1; i < lines.size(); ++i) {
        try {
            const QStringList cells = parseCsvLine(lines[i]);
            if (cells.size() < 7) {
                ++skipped;
                continue;
            }
            TxFrame frame;
            frame.name = cells[0].trimmed();
            frame.idText = cells[1].trimmed();
            frame.extendedFrame = cells[2].trimmed() == "1";
            frame.remoteFrame = cells[3].trimmed() == "1";
            frame.cycleEnabled = cells[4].trimmed() == "1";
            frame.payloadText = cells[6].trimmed();
            frame.intervalMs = cells.size() >= 8 ? std::max(10, cells[7].trimmed().toInt()) : 200;

            const quint32 canId = UsbCanService::parseCanId(frame.idText);
            const QByteArray payload = UsbCanService::parsePayload(frame.payloadText);
            if (frame.name.isEmpty()) {
                frame.name = QString("F%1").arg(++txFrameCounter_);
            }
            frame.idText = UsbCanService::formatId(canId, frame.extendedFrame);
            frame.payloadText = UsbCanService::formatBytes(payload);

            addTxRow(frame);
            if (frame.cycleEnabled && service_.isConnected() && !nmeaMode_) {
                startCycleLoop(cycleButtonAt(txTable_->rowCount() - 1));
            }
            ++imported;
        } catch (...) {
            ++skipped;
        }
    }
    appendLog(QString("Import TX CSV complete. Imported: %1, Skipped: %2").arg(imported).arg(skipped));
}

void SessionWidget::exportRxCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Receive Frames", canToolDocumentsPath("rx_frames.csv"), "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(QString("Export RX CSV failed: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "Realtime,Seen,FrequencyHz,Channel,ID,Type,Length,Data";
    out << (dbcMode_ ? ",Message,Signals\n" : "\n");
    for (const CanFrame& frame : activeRxFrames()) {
        if (!frameMatchesFilters(frame)) {
            continue;
        }
        const CanFrame displayFrame = frameWithCurrentRxStats(frame);
        out << csv(displayFrame.timestamp) << ','
            << displayFrame.seenCount << ','
            << QString::number(displayFrame.receiveFrequencyHz, 'f', 2) << ','
            << csv(displayFrame.channel) << ','
            << csv(displayFrame.idText) << ','
            << csv(displayFrame.frameType) << ','
            << displayFrame.length << ','
            << csv(displayFrame.dataText);
        if (dbcMode_) {
            const int message = dbc_.findMessage(displayFrame.rawId, displayFrame.frameType == "EXT", dbcJ1939Check_->isChecked());
            if (message >= 0) {
                const Dbc::Message& definition = dbc_.messages()[message];
                out << ',' << csv(definition.name) << ',' << csv(Dbc::formatSignals(definition, Dbc::decode(definition, displayFrame.dataBytes)));
            } else {
                out << ",,";
            }
        }
        out << '\n';
    }
    appendLog(QString("RX CSV exported: %1").arg(path));
}

void SessionWidget::exportLogCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Event Log", canToolDocumentsPath("event_log.csv"), "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(QString("Export Log CSV failed: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "Time,Message\n";
    const QStringList lines = logText_->toPlainText().split(QRegularExpression("\\r?\\n"), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const int split = line.indexOf("  ");
        const QString time = split > 0 ? line.left(split).trimmed() : QString();
        const QString message = split > 0 ? line.mid(split + 2).trimmed() : line.trimmed();
        out << csv(time) << ',' << csv(message) << '\n';
    }
    appendLog(QString("Log CSV exported: %1").arg(path));
}

void SessionWidget::onFramesReceived(const QVector<CanFrame>& frames)
{
    if (nmeaMode_ || frames.isEmpty()) {
        return;
    }

    pendingRxFrames_ += frames;
    scheduleRxFlush(RxFlushIntervalMs);
}

void SessionWidget::onRawFramesReceived(const QVector<CanFrame>& frames)
{
    if (!nmeaMode_ || frames.isEmpty()) {
        return;
    }

    QVector<CanFrame> decodedFrames;
    decodedFrames.reserve(frames.size());
    for (const CanFrame& frame : frames) {
        Nmea2000::RawFrame raw;
        raw.timestampMs = frame.lastSeenMs;
        raw.timestamp = frame.timestamp;
        raw.canId = frame.rawId;
        raw.extended = frame.frameType == "EXT";
        raw.data = frame.dataBytes;

        const Nmea2000::Info info = nmeaDecoder_.process(raw);
        if (!info.valid) {
            continue;
        }

        CanFrame decoded = frame;
        decoded.nmeaValid = true;
        decoded.nmeaPgn = info.pgn;
        decoded.nmeaPgnText = info.pgnText;
        decoded.nmeaName = info.pgnName;
        decoded.nmeaPriority = info.priority;
        decoded.nmeaSource = info.source;
        decoded.nmeaDestination = info.destination;
        decoded.nmeaTransport = info.transport;
        decoded.nmeaDecoded = info.decoded;
        const QString schemaDecoded = NmeaDictionary::decode(info.pgn, info.payload);
        if (!schemaDecoded.isEmpty()) {
            decoded.nmeaDecoded = schemaDecoded;
        }
        decodedFrames.push_back(decoded);
    }

    for (auto it = decodedFrames.crbegin(); it != decodedFrames.crend(); ++it) {
        nmeaFrames_.prepend(*it);
        const QString key = groupedKey(*it);
        const auto grouped = nmeaGroupedIndex_.constFind(key);
        if (grouped == nmeaGroupedIndex_.cend()) {
            nmeaGroupedFrames_.prepend(*it);
            nmeaGroupedIndex_.clear();
            for (int index = 0; index < nmeaGroupedFrames_.size(); ++index) {
                nmeaGroupedIndex_[groupedKey(nmeaGroupedFrames_[index])] = index;
            }
        } else {
            nmeaGroupedFrames_[grouped.value()] = *it;
        }
    }
    if (nmeaFrames_.size() > 500) {
        nmeaFrames_.remove(500, nmeaFrames_.size() - 500);
    }
    refreshRxTable();
}

void SessionWidget::flushPendingRxFrames()
{
    rxFlushScheduled_ = false;
    if (pendingRxFrames_.isEmpty()) {
        return;
    }

    const int takeCount = qMin(MaxFramesPerRxFlush, pendingRxFrames_.size());
    QVector<CanFrame> frames;
    frames.reserve(takeCount);
    for (int i = 0; i < takeCount; ++i) {
        frames.push_back(pendingRxFrames_[i]);
    }
    pendingRxFrames_.remove(0, takeCount);
    if (dbcMode_) {
        updateDbcSignals(frames);
    }

    QVector<CanFrame> displayFrames;
    displayFrames.reserve(qMin(500, frames.size() + allFrames_.size()));
    for (int i = frames.size() - 1; i >= 0; --i) {
        const CanFrame& frame = frames[i];
        const CanFrame enrichedFrame = applyRxStats(frame);
        if (displayFrames.size() < 500) {
            displayFrames.push_back(enrichedFrame);
        }
        upsertGroupedFrame(enrichedFrame);
    }

    for (const CanFrame& frame : allFrames_) {
        if (displayFrames.size() >= 500) {
            break;
        }
        displayFrames.push_back(frame);
    }
    allFrames_.swap(displayFrames);

    refreshRxTable();
    if (watchPanel_->isVisible()) {
        refreshWatchValue();
    }

    if (!pendingRxFrames_.isEmpty()) {
        scheduleRxFlush(RxCatchUpFlushDelayMs);
    }
}

void SessionWidget::onLogEmitted(const QString& message)
{
    appendLog(message);
}

void SessionWidget::updateConnectionState()
{
    const bool connected = service_.isConnected();
    connectButton_->setEnabled(!connected);
    disconnectButton_->setEnabled(connected);
    const QString connectionText = connected
        ? (nmeaMode_ ? QString("NMEA / %1").arg(modeCombo_->currentData().toString().toUpper()) : QStringLiteral("CONNECTED"))
        : QStringLiteral("DISCONNECTED");
    connectionStatus_->setText(localize(language_, connectionText));
    connectionStatus_->setProperty("state", connected ? "success" : "inactive");
    connectionStatus_->style()->unpolish(connectionStatus_);
    connectionStatus_->style()->polish(connectionStatus_);
    connectionStatus_->update();
    updateOperatingModeUi();
    updateTxQueueButton();
}

void SessionWidget::updateOperatingModeUi()
{
    if (!profileCombo_) {
        return;
    }

    const bool connected = service_.isConnected();
    const bool nmea = nmeaMode_;
    baudCombo_->setEnabled(!nmea);
    modeCombo_->setEnabled(true);
    filterCombo_->setEnabled(!nmea);
    // Grouping is a display option and remains available for NMEA traffic.
    groupFramesCheck_->setEnabled(true);
    txBox_->setEnabled(!nmea);
    txBox_->setTitle(localize(language_, nmea ? "Transmit (disabled in NMEA 2000)" : "Transmit"));
    profileCombo_->setToolTip(localize(language_, nmea
        ? "NMEA 2000 monitor: 250 kbit/s, extended IDs, accept all; Normal mode ACKs frames, Listen Only does not"
        : (dbcMode_ ? "DBC profile: use the selected adapter settings and decode frames with the loaded DBC"
                    : "Fault reading profile: use the selected adapter settings")));
    rxModel_->setMode(nmea ? RxViewMode::Nmea : (dbcMode_ ? RxViewMode::Dbc : RxViewMode::Fault));
    for (int column = 0; column < rxModel_->columnCount(); ++column) {
        rxTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Interactive);
    }
    rxTable_->horizontalHeader()->setSectionResizeMode(rxModel_->stretchColumn(), QHeaderView::Stretch);
    dbcBar_->setVisible(dbcMode_);
    dbcSignalsPanel_->setVisible(dbcMode_);
    if (nmea && connected) {
        connectionStatus_->setProperty("state", "success");
    }
}

void SessionWidget::appendLog(const QString& message)
{
    logText_->append(message);
}

void SessionWidget::addTxRow(const TxFrame& frame)
{
    const int row = txTable_->rowCount();
    txTable_->insertRow(row);
    txTable_->setRowHeight(row, 34);

    auto* actionWidget = new QWidget(txTable_);
    actionWidget->setMinimumWidth(158);
    auto* actionLayout = new QHBoxLayout(actionWidget);
    actionLayout->setContentsMargins(5, 2, 5, 2);
    actionLayout->setSpacing(6);
    auto* manualButton = new QToolButton(actionWidget);
    auto* cycleButton = new QToolButton(actionWidget);
    manualButton->setObjectName("manualButton");
    cycleButton->setObjectName("cycleButton");
    manualButton->setProperty("i18nSource", "Send");
    cycleButton->setProperty("cycleControl", true);
    manualButton->setText(localize(language_, "Send"));
    cycleButton->setText(localize(language_, frame.cycleEnabled ? "Stop" : "Cycle"));
    manualButton->setToolTip("Send this frame once");
    cycleButton->setToolTip("Toggle cyclic sending for this frame");
    manualButton->setFixedSize(58, 20);
    cycleButton->setFixedSize(62, 20);
    cycleButton->setCheckable(true);
    cycleButton->setChecked(frame.cycleEnabled);
    actionLayout->addWidget(manualButton);
    actionLayout->addWidget(cycleButton);
    txTable_->setCellWidget(row, TxActionColumn, actionWidget);

    connect(manualButton, &QToolButton::clicked, this, [this, cycleButton] {
        manualSendRow(rowForCycleButton(cycleButton));
    });
    connect(cycleButton, &QToolButton::toggled, this, [this, cycleButton](bool checked) {
        if (checked) {
            cycleButton->setText(localize(language_, "Stop"));
            startCycleLoop(cycleButton);
            if (!cycleTimers_.contains(cycleButton)) {
                cycleButton->setText(localize(language_, "Cycle"));
            }
        } else {
            cycleButton->setText(localize(language_, "Cycle"));
            stopCycleLoop(cycleButton);
        }
    });

    auto* name = new QTableWidgetItem(frame.name);
    auto* ext = new QTableWidgetItem();
    auto* rtr = new QTableWidgetItem();
    auto* interval = new QTableWidgetItem(QString::number(frame.intervalMs));
    auto* id = new QTableWidgetItem(frame.idText);
    auto* data = new QTableWidgetItem(frame.payloadText);
    ext->setCheckState(frame.extendedFrame ? Qt::Checked : Qt::Unchecked);
    rtr->setCheckState(frame.remoteFrame ? Qt::Checked : Qt::Unchecked);

    txTable_->setItem(row, TxNameColumn, name);
    txTable_->setItem(row, TxExtColumn, ext);
    txTable_->setItem(row, TxRtrColumn, rtr);
    txTable_->setItem(row, TxIntervalColumn, interval);
    txTable_->setItem(row, TxIdColumn, id);
    txTable_->setItem(row, TxDataColumn, data);
    updateTxQueueButton();
}

TxFrame SessionWidget::readTxRow(int row) const
{
    if (row < 0 || row >= txTable_->rowCount()) {
        throw std::runtime_error("Select a transmit frame first.");
    }

    TxFrame frame;
    frame.name = txTable_->item(row, TxNameColumn)->text();
    frame.extendedFrame = txTable_->item(row, TxExtColumn)->checkState() == Qt::Checked;
    frame.remoteFrame = txTable_->item(row, TxRtrColumn)->checkState() == Qt::Checked;
    frame.intervalMs = std::max(10, txTable_->item(row, TxIntervalColumn)->text().toInt());
    frame.idText = txTable_->item(row, TxIdColumn)->text();
    frame.payloadText = txTable_->item(row, TxDataColumn)->text();
    if (auto* button = cycleButtonAt(row)) {
        frame.cycleEnabled = button->isChecked();
    }
    return frame;
}

int SessionWidget::selectedTxRow() const
{
    const auto ranges = txTable_->selectedRanges();
    return ranges.isEmpty() ? -1 : ranges.first().topRow();
}

int SessionWidget::rowForCycleButton(QToolButton* button) const
{
    for (int row = 0; row < txTable_->rowCount(); ++row) {
        if (cycleButtonAt(row) == button) {
            return row;
        }
    }
    return -1;
}

QToolButton* SessionWidget::cycleButtonAt(int row) const
{
    if (auto* cell = txTable_->cellWidget(row, TxActionColumn)) {
        return cell->findChild<QToolButton*>("cycleButton");
    }
    return nullptr;
}

void SessionWidget::manualSendRow(int row)
{
    if (nmeaMode_) {
        appendLog("Manual transmit is disabled in NMEA 2000 monitoring mode.");
        return;
    }
    if (!service_.isConnected()) {
        appendLog("Manual send requires an active connection.");
        return;
    }

    try {
        const TxFrame frame = readTxRow(row);
        service_.sendFrame(UsbCanService::parseCanId(frame.idText), UsbCanService::parsePayload(frame.payloadText), frame.extendedFrame, frame.remoteFrame);
    } catch (const std::exception& ex) {
        appendLog(QString("Manual send error: %1").arg(ex.what()));
    }
}

void SessionWidget::startTxQueueLoop()
{
    if (nmeaMode_) {
        appendLog("Loop Queue is disabled in NMEA 2000 monitoring mode.");
        updateTxQueueButton();
        return;
    }
    if (!service_.isConnected()) {
        appendLog("Loop Queue requires an active connection.");
        updateTxQueueButton();
        return;
    }
    if (txTable_->rowCount() == 0) {
        appendLog("Loop Queue requires at least one transmit row.");
        updateTxQueueButton();
        return;
    }
    if (txQueueTimer_) {
        updateTxQueueButton();
        return;
    }

    txQueueRow_ = 0;
    txQueueTimer_ = new QTimer(this);
    txQueueTimer_->setInterval(200);
    connect(txQueueTimer_, &QTimer::timeout, this, &SessionWidget::sendNextTxQueueFrame);
    txQueueTimer_->start();
    sendNextTxQueueFrame();
    updateTxQueueButton();
}

void SessionWidget::stopTxQueueLoop()
{
    if (txQueueTimer_) {
        txQueueTimer_->stop();
        txQueueTimer_->deleteLater();
        txQueueTimer_ = nullptr;
    }
    txQueueRow_ = 0;
    updateTxQueueButton();
}

void SessionWidget::sendNextTxQueueFrame()
{
    if (nmeaMode_ || !service_.isConnected() || txTable_->rowCount() == 0) {
        stopTxQueueLoop();
        return;
    }

    if (txQueueRow_ < 0 || txQueueRow_ >= txTable_->rowCount()) {
        txQueueRow_ = 0;
    }

    const int row = txQueueRow_;
    txQueueRow_ = (txQueueRow_ + 1) % txTable_->rowCount();

    try {
        const TxFrame frame = readTxRow(row);
        service_.sendFrame(UsbCanService::parseCanId(frame.idText), UsbCanService::parsePayload(frame.payloadText), frame.extendedFrame, frame.remoteFrame);
    } catch (const std::exception& ex) {
        appendLog(QString("Loop Queue send error on row %1: %2").arg(row + 1).arg(ex.what()));
        stopTxQueueLoop();
    }
}

void SessionWidget::startCycleLoop(QToolButton* button)
{
    if (nmeaMode_ || !button || !button->isChecked() || !service_.isConnected() || cycleTimers_.contains(button)) {
        if (nmeaMode_ && button) {
            button->setChecked(false);
        }
        return;
    }

    auto* timer = new QTimer(this);
    cycleTimers_[button] = timer;
    connect(timer, &QTimer::timeout, this, [this, button, timer] {
        const int row = rowForCycleButton(button);
        if (row < 0 || !service_.isConnected()) {
            stopCycleLoop(button);
            return;
        }
        try {
            const TxFrame frame = readTxRow(row);
            service_.sendFrame(UsbCanService::parseCanId(frame.idText), UsbCanService::parsePayload(frame.payloadText), frame.extendedFrame, frame.remoteFrame);
            timer->setInterval(std::max(10, frame.intervalMs));
        } catch (const std::exception& ex) {
            appendLog(QString("Cycle send error: %1").arg(ex.what()));
            stopCycleLoop(button);
        }
    });
    timer->start(std::max(10, readTxRow(rowForCycleButton(button)).intervalMs));
}

void SessionWidget::stopCycleLoop(QToolButton* button)
{
    if (!button) {
        return;
    }
    if (auto* timer = cycleTimers_.take(button)) {
        timer->stop();
        timer->deleteLater();
    }
    button->setChecked(false);
}

void SessionWidget::stopAllCycleLoops()
{
    const auto buttons = cycleTimers_.keys();
    for (QToolButton* button : buttons) {
        stopCycleLoop(button);
    }
}

void SessionWidget::updateTxQueueButton()
{
    if (!txQueueButton_ || !txTable_) {
        return;
    }

    const bool hasRows = txTable_->rowCount() > 0;
    const bool queueRunning = txQueueTimer_ != nullptr;
    const QSignalBlocker blocker(txQueueButton_);
    txQueueButton_->setEnabled(!nmeaMode_ && service_.isConnected() && hasRows);
    txQueueButton_->setChecked(queueRunning);
    txQueueButton_->setText(localize(language_, queueRunning ? "Stop Queue" : "Loop Queue"));
}

void SessionWidget::upsertGroupedFrame(const CanFrame& frame)
{
    const QString key = groupedKey(frame);
    if (groupedIndex_.contains(key)) {
        CanFrame& existing = groupedFrames_[groupedIndex_[key]];
        existing = frame;
        return;
    }

    groupedFrames_.prepend(frame);
    rebuildGroupedIndex();
}

void SessionWidget::rebuildGroupedIndex()
{
    groupedIndex_.clear();
    for (int i = 0; i < groupedFrames_.size(); ++i) {
        groupedIndex_[groupedKey(groupedFrames_[i])] = i;
    }
}

QString SessionWidget::groupedKey(const CanFrame& frame) const
{
    return QString("%1|%2|%3").arg(frame.channel, QString::number(frame.rawId, 16), frame.frameType);
}

CanFrame SessionWidget::applyRxStats(const CanFrame& frame)
{
    const QString key = groupedKey(frame);
    RxFrameStats& stats = rxStats_[key];
    const int receivedCount = std::max(1, frame.seenCount);
    if (stats.seenCount == 0) {
        stats.firstSeenTimestamp = frame.timestamp;
        stats.firstSeenMs = frame.lastSeenMs;
    }

    stats.seenCount += receivedCount;
    stats.lastSeenTimestamp = frame.timestamp;
    stats.lastSeenMs = frame.lastSeenMs;
    stats.receiveFrequencyHz = receiveFrequencyHz(stats.seenCount, stats.firstSeenMs, stats.lastSeenMs);

    CanFrame enrichedFrame = frame;
    applyRxStatsToFrame(enrichedFrame, stats);
    return enrichedFrame;
}

CanFrame SessionWidget::frameWithCurrentRxStats(const CanFrame& frame) const
{
    if (nmeaMode_) {
        return frame;
    }

    CanFrame displayFrame = frame;
    const auto stats = rxStats_.constFind(groupedKey(frame));
    if (stats != rxStats_.cend()) {
        applyRxStatsToFrame(displayFrame, stats.value());
    }
    return displayFrame;
}

void SessionWidget::applyRxStatsToFrame(CanFrame& frame, const RxFrameStats& stats)
{
    frame.firstSeenTimestamp = stats.firstSeenTimestamp;
    frame.lastSeenTimestamp = stats.lastSeenTimestamp;
    frame.firstSeenMs = stats.firstSeenMs;
    frame.lastSeenMs = stats.lastSeenMs;
    frame.seenCount = stats.seenCount;
    frame.receiveFrequencyHz = stats.receiveFrequencyHz;
}

bool SessionWidget::frameMatchesFilters(const CanFrame& frame) const
{
    const QString idFilter = rxFilterId_->text().trimmed();
    if (!idFilter.isEmpty()) {
        QString normalizedFilter = idFilter;
        if (normalizedFilter.startsWith("0x", Qt::CaseInsensitive)) {
            normalizedFilter = normalizedFilter.mid(2);
        }
        const QString normalizedId = frame.idText.startsWith("0x", Qt::CaseInsensitive)
            ? frame.idText.mid(2)
            : frame.idText;
        const QString pgnText = frame.nmeaValid ? frame.nmeaPgnText : QString();
        const QString pgnDecimal = frame.nmeaValid ? QString::number(frame.nmeaPgn) : QString();
        QString dbcName;
        if (dbcMode_) {
            const int message = dbc_.findMessage(frame.rawId, frame.frameType == "EXT", dbcJ1939Check_->isChecked());
            if (message >= 0) {
                dbcName = dbc_.messages()[message].name;
            }
        }
        if (!normalizedId.contains(normalizedFilter, Qt::CaseInsensitive)
            && !pgnText.contains(idFilter, Qt::CaseInsensitive)
            && !pgnDecimal.contains(idFilter, Qt::CaseInsensitive)
            && !dbcName.contains(idFilter, Qt::CaseInsensitive)) {
            return false;
        }
    }

    const QString type = rxFilterType_->currentData().toString();
    if (type != "ALL" && frame.frameType != type) {
        return false;
    }

    const QString payloadFilter = rxFilterPayload_->text().trimmed();
    if (!payloadFilter.isEmpty() && !frame.dataText.contains(payloadFilter, Qt::CaseInsensitive)) {
        return false;
    }

    return true;
}

void SessionWidget::refreshRxTable()
{
    QVector<CanFrame> rows;
    rows.reserve(qMin(500, activeRxFrames().size()));
    for (const CanFrame& frame : activeRxFrames()) {
        if (!frameMatchesFilters(frame)) {
            continue;
        }
        rows.push_back(frameWithCurrentRxStats(frame));
    }
    rxModel_->setFrames(std::move(rows));
}

const QVector<CanFrame>& SessionWidget::activeRxFrames() const
{
    if (nmeaMode_) {
        return groupFramesCheck_->isChecked() ? nmeaGroupedFrames_ : nmeaFrames_;
    }
    return groupFramesCheck_->isChecked() ? groupedFrames_ : allFrames_;
}

void SessionWidget::scheduleRxFlush(int delayMs)
{
    if (rxFlushScheduled_) {
        return;
    }

    rxFlushScheduled_ = true;
    QTimer::singleShot(delayMs, this, &SessionWidget::flushPendingRxFrames);
}

bool SessionWidget::updateWatchValue(const CanFrame& frame)
{
    try {
        quint32 watchId = 0;
        int startByte = 0;
        int length = 1;
        bool littleEndian = true;
        bool isSigned = false;
        if (!tryReadWatchConfig(watchId, startByte, length, littleEndian, isSigned)) {
            watchRawValue_->setText("Invalid config");
            watchComputedValue_->setText("-");
            return false;
        }
        if (frame.rawId != watchId) {
            return false;
        }
        if (startByte < 0 || length <= 0 || startByte + length > frame.dataBytes.size()) {
            watchRawValue_->setText("Out of range");
            watchComputedValue_->setText("-");
            return true;
        }

        QByteArray bytes = frame.dataBytes.mid(startByte, length);
        if (!littleEndian) {
            std::reverse(bytes.begin(), bytes.end());
        }

        qint64 raw = 0;
        for (int i = 0; i < bytes.size(); ++i) {
            raw |= static_cast<qint64>(static_cast<unsigned char>(bytes[i])) << (8 * i);
        }
        if (isSigned) {
            const int bits = length * 8;
            const qint64 signBit = 1LL << (bits - 1);
            if ((raw & signBit) != 0) {
                raw -= 1LL << bits;
            }
        }

        watchRawValue_->setText(QString::number(raw));
        const double computed = FormulaEvaluator::evaluate(static_cast<double>(raw), watchFormula_->text());
        QString computedText = QString::number(computed, 'f', 6)
            .remove(QRegularExpression("0+$"))
            .remove(QRegularExpression("\\.$"));
        if (computedText.isEmpty() || computedText == "-") {
            computedText = "0";
        }
        watchComputedValue_->setText(computedText);
        return true;
    } catch (const std::exception& ex) {
        watchComputedValue_->setText(QString("Err: %1").arg(ex.what()));
        return true;
    }
}

void SessionWidget::refreshWatchValue()
{
    if (!watchPanel_ || !watchPanel_->isVisible()) {
        return;
    }

    quint32 watchId = 0;
    int startByte = 0;
    int length = 1;
    bool littleEndian = true;
    bool isSigned = false;
    if (!tryReadWatchConfig(watchId, startByte, length, littleEndian, isSigned)) {
        watchRawValue_->setText("Invalid config");
        watchComputedValue_->setText("-");
        return;
    }

    for (const CanFrame& frame : allFrames_) {
        if (updateWatchValue(frame)) {
            return;
        }
    }

    watchRawValue_->setText("-");
    watchComputedValue_->setText("No match");
}

bool SessionWidget::tryReadWatchConfig(quint32& watchId, int& startByte, int& length, bool& littleEndian, bool& isSigned) const
{
    try {
        watchId = UsbCanService::parseCanId(watchId_->text());
        bool startByteOk = false;
        startByte = watchStartByte_->text().toInt(&startByteOk);
        if (!startByteOk) {
            return false;
        }
        length = watchLength_->currentText().toInt();
        littleEndian = watchEndian_->currentData().toString() == "Little";
        isSigned = watchSigned_->isChecked();
        return true;
    } catch (...) {
        return false;
    }
}

QString SessionWidget::csv(const QString& value)
{
    if (value.contains('"') || value.contains(',') || value.contains('\n') || value.contains('\r')) {
        QString escaped = value;
        escaped.replace("\"", "\"\"");
        return QString("\"%1\"").arg(escaped);
    }
    return value;
}

QStringList SessionWidget::parseCsvLine(const QString& line)
{
    QStringList result;
    QString cell;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                cell.append('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            result << cell;
            cell.clear();
        } else {
            cell.append(c);
        }
    }
    result << cell;
    return result;
}

QString SessionWidget::canToolDocumentsPath(const QString& fileName)
{
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documentsPath.isEmpty()) {
        documentsPath = QDir::homePath();
    }

    QDir documentsDir(documentsPath);
    if (!documentsDir.exists("CanToolDoc")) {
        documentsDir.mkpath("CanToolDoc");
    }

    const QString directoryPath = documentsDir.filePath("CanToolDoc");
    return fileName.isEmpty() ? directoryPath : QDir(directoryPath).filePath(fileName);
}

double SessionWidget::receiveFrequencyHz(int seenCount, qint64 firstSeenMs, qint64 lastSeenMs)
{
    const qint64 elapsedMs = lastSeenMs - firstSeenMs;
    if (seenCount < 2 || elapsedMs < MinFrequencyElapsedMs) {
        return 0.0;
    }

    return (seenCount - 1) / (elapsedMs / 1000.0);
}

QString SessionWidget::formatFrequency(double hz)
{
    return QString::number(hz, 'f', 2);
}
