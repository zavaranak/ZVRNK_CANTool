#include "session_widget.h"

#include "control_can_native.h"
#include "formula_evaluator.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFile>
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
#include <QSizePolicy>
#include <QSplitter>
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
}

SessionWidget::SessionWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyStyle();

    connect(&service_, &UsbCanService::frameReceived, this, &SessionWidget::onFrameReceived);
    connect(&service_, &UsbCanService::logEmitted, this, &SessionWidget::onLogEmitted);

    refreshDevices();
    updateConnectionState();
}

SessionWidget::~SessionWidget()
{
    shutdown();
}

void SessionWidget::shutdown()
{
    stopAllCycleLoops();
    service_.disconnectDevice();
}

void SessionWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto* verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->setObjectName("sectionSplitter");
    verticalSplitter->setChildrenCollapsible(true);
    verticalSplitter->setOpaqueResize(true);
    verticalSplitter->setHandleWidth(9);
    root->addWidget(verticalSplitter, 1);

    auto* header = new QGroupBox(this);
    relaxSplitterChild(header);
    auto* headerLayout = new QGridLayout(header);
    headerLayout->setSizeConstraint(QLayout::SetNoConstraint);

    deviceCombo_ = new QComboBox(header);
    channelCombo_ = new QComboBox(header);
    baudCombo_ = new QComboBox(header);
    modeCombo_ = new QComboBox(header);
    filterCombo_ = new QComboBox(header);
    connectButton_ = new QPushButton("Connect", header);
    disconnectButton_ = new QPushButton("Disconnect", header);
    auto* refreshButton = new QPushButton("Refresh", header);
    auto* closeButton = new QPushButton("Close Session", header);

    channelCombo_->addItems({"0", "1"});
    baudCombo_->addItems({"1M", "500K", "250K", "125K"});
    baudCombo_->setCurrentText("500K");
    modeCombo_->addItems({"Normal", "Listen Only"});
    filterCombo_->addItems({"Accept All", "Single Filter"});

    const auto addLabeled = [headerLayout](const QString& label, QWidget* widget, int column) {
        auto* text = new QLabel(label);
        headerLayout->addWidget(text, 0, column);
        headerLayout->addWidget(widget, 1, column);
    };
    addLabeled("Device", deviceCombo_, 0);
    addLabeled("Channel", channelCombo_, 1);
    addLabeled("Baud", baudCombo_, 2);
    addLabeled("Mode", modeCombo_, 3);
    addLabeled("Filter", filterCombo_, 4);
    headerLayout->addWidget(refreshButton, 1, 5);
    headerLayout->addWidget(connectButton_, 1, 6);
    headerLayout->addWidget(disconnectButton_, 1, 7);
    headerLayout->addWidget(closeButton, 1, 8);
    headerLayout->setColumnStretch(0, 1);
    verticalSplitter->addWidget(header);

    connect(refreshButton, &QPushButton::clicked, this, &SessionWidget::refreshDevices);
    connect(connectButton_, &QPushButton::clicked, this, &SessionWidget::connectDevice);
    connect(disconnectButton_, &QPushButton::clicked, this, &SessionWidget::disconnectDevice);
    connect(closeButton, &QPushButton::clicked, this, &SessionWidget::closeRequested);

    auto* middleSplitter = new QSplitter(Qt::Horizontal, this);
    middleSplitter->setObjectName("panelSplitter");
    middleSplitter->setChildrenCollapsible(true);
    middleSplitter->setOpaqueResize(true);
    middleSplitter->setHandleWidth(9);
    verticalSplitter->addWidget(middleSplitter);

    auto* txBox = new QGroupBox("Transmit", this);
    relaxSplitterChild(txBox);
    auto* txLayout = new QVBoxLayout(txBox);
    txLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* txActions = new QHBoxLayout();
    auto* addButton = new QPushButton("Add", txBox);
    auto* removeButton = new QPushButton("Remove", txBox);
    auto* exportTxButton = new QPushButton("Export CSV", txBox);
    auto* importTxButton = new QPushButton("Import CSV", txBox);
    defaultExtCheck_ = new QCheckBox("Extended Frame", txBox);
    defaultRtrCheck_ = new QCheckBox("Remote Frame", txBox);
    defaultExtCheck_->setChecked(true);
    txActions->addWidget(defaultExtCheck_);
    txActions->addWidget(defaultRtrCheck_);
    txActions->addStretch();
    txActions->addWidget(addButton);
    txActions->addWidget(removeButton);
    txActions->addWidget(exportTxButton);
    txActions->addWidget(importTxButton);
    txLayout->addLayout(txActions);

    txTable_ = new QTableWidget(0, TxColumnCount, txBox);
    relaxTable(txTable_);
    txTable_->setHorizontalHeaderLabels({"M/C", "Name", "Ext", "RTR", "Interval(ms)", "ID", "Data"});
    txTable_->horizontalHeader()->setSectionResizeMode(TxDataColumn, QHeaderView::Stretch);
    txTable_->verticalHeader()->setVisible(false);
    txTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    txTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    txLayout->addWidget(txTable_, 1);
    middleSplitter->addWidget(txBox);

    connect(addButton, &QPushButton::clicked, this, &SessionWidget::addTxFrame);
    connect(removeButton, &QPushButton::clicked, this, &SessionWidget::removeTxFrame);
    connect(exportTxButton, &QPushButton::clicked, this, &SessionWidget::exportTxCsv);
    connect(importTxButton, &QPushButton::clicked, this, &SessionWidget::importTxCsv);

    auto* rxBox = new QGroupBox("Received Frames", this);
    relaxSplitterChild(rxBox);
    auto* rxLayout = new QVBoxLayout(rxBox);
    rxLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* rxTop = new QHBoxLayout();
    auto* exportRxButton = new QPushButton("Export CSV", rxBox);
    auto* watchButton = new QPushButton("Watch", rxBox);
    watchButton->setCheckable(true);
    auto* clearRxButton = new QPushButton("Clear List", rxBox);
    rxTop->addStretch();
    rxTop->addWidget(exportRxButton);
    rxTop->addWidget(watchButton);
    rxTop->addWidget(clearRxButton);
    rxLayout->addLayout(rxTop);

    auto* filters = new QHBoxLayout();
    rxFilterId_ = new QLineEdit(rxBox);
    rxFilterType_ = new QComboBox(rxBox);
    rxFilterPayload_ = new QLineEdit(rxBox);
    groupFramesCheck_ = new QCheckBox("Group Unique", rxBox);
    auto* resetFilterButton = new QPushButton("Reset", rxBox);
    rxFilterType_->addItems({"ALL", "STD", "EXT", "RTR"});
    rxFilterId_->setPlaceholderText("Filter ID");
    rxFilterPayload_->setPlaceholderText("Payload Contains");
    filters->addWidget(rxFilterId_);
    filters->addWidget(rxFilterType_);
    filters->addWidget(rxFilterPayload_);
    filters->addWidget(groupFramesCheck_);
    filters->addWidget(resetFilterButton);
    rxLayout->addLayout(filters);

    watchPanel_ = new QWidget(rxBox);
    relaxSplitterChild(watchPanel_);
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
    watchEndian_->addItems({"Little", "Big"});
    watchLayout->addWidget(new QLabel("Watch ID"), 0, 0);
    watchLayout->addWidget(new QLabel("Start Byte"), 0, 1);
    watchLayout->addWidget(new QLabel("Length"), 0, 2);
    watchLayout->addWidget(new QLabel("Endian"), 0, 3);
    watchLayout->addWidget(new QLabel("Formula (x = dec)"), 0, 5);
    watchLayout->addWidget(watchId_, 1, 0);
    watchLayout->addWidget(watchStartByte_, 1, 1);
    watchLayout->addWidget(watchLength_, 1, 2);
    watchLayout->addWidget(watchEndian_, 1, 3);
    watchLayout->addWidget(watchSigned_, 1, 4);
    watchLayout->addWidget(watchFormula_, 1, 5);
    watchLayout->addWidget(new QLabel("Raw Value (dec)"), 0, 6);
    watchLayout->addWidget(watchRawValue_, 1, 6);
    watchLayout->addWidget(new QLabel("Computed Value"), 0, 7);
    watchLayout->addWidget(watchComputedValue_, 1, 7);
    watchPanel_->setVisible(false);
    rxLayout->addWidget(watchPanel_);

    rxTable_ = new QTableWidget(0, 7, rxBox);
    relaxTable(rxTable_);
    rxTable_->setHorizontalHeaderLabels({"Time", "Seen", "Channel", "ID", "Type", "Len", "Data"});
    rxTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    rxTable_->verticalHeader()->setVisible(false);
    rxTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rxTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rxLayout->addWidget(rxTable_, 1);
    middleSplitter->addWidget(rxBox);
    middleSplitter->setStretchFactor(0, 4);
    middleSplitter->setStretchFactor(1, 6);

    connect(exportRxButton, &QPushButton::clicked, this, &SessionWidget::exportRxCsv);
    connect(watchButton, &QPushButton::toggled, watchPanel_, &QWidget::setVisible);
    connect(clearRxButton, &QPushButton::clicked, this, &SessionWidget::clearRxFrames);
    connect(resetFilterButton, &QPushButton::clicked, this, &SessionWidget::clearRxFilters);
    connect(rxFilterId_, &QLineEdit::textChanged, this, &SessionWidget::refreshRxTable);
    connect(rxFilterPayload_, &QLineEdit::textChanged, this, &SessionWidget::refreshRxTable);
    connect(rxFilterType_, &QComboBox::currentTextChanged, this, &SessionWidget::refreshRxTable);
    connect(groupFramesCheck_, &QCheckBox::toggled, this, &SessionWidget::refreshRxTable);

    auto* logBox = new QGroupBox("Event Log", this);
    relaxSplitterChild(logBox);
    auto* logLayout = new QVBoxLayout(logBox);
    logLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto* logActions = new QHBoxLayout();
    auto* exportLogButton = new QPushButton("Export CSV", logBox);
    auto* clearLogButton = new QPushButton("Clear Log", logBox);
    logActions->addStretch();
    logActions->addWidget(exportLogButton);
    logActions->addWidget(clearLogButton);
    logLayout->addLayout(logActions);
    logText_ = new QTextEdit(logBox);
    relaxSplitterChild(logText_);
    logText_->setReadOnly(true);
    logLayout->addWidget(logText_);
    verticalSplitter->addWidget(logBox);
    verticalSplitter->setStretchFactor(0, 0);
    verticalSplitter->setStretchFactor(1, 6);
    verticalSplitter->setStretchFactor(2, 2);
    verticalSplitter->setSizes({140, 480, 170});

    connect(exportLogButton, &QPushButton::clicked, this, &SessionWidget::exportLogCsv);
    connect(clearLogButton, &QPushButton::clicked, this, &SessionWidget::clearLog);
}

void SessionWidget::applyStyle()
{
    setStyleSheet(R"(
        QWidget { background: #0E1624; color: #FFFFFF; }
        QGroupBox { background: #162338; border: 1px solid #2E4568; border-radius: 8px; margin-top: 12px; padding: 12px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #FFFFFF; }
        QLineEdit, QComboBox, QTextEdit, QTableWidget { background: #0E1624; border: 1px solid #2E4568; color: #FFFFFF; selection-background-color: #2B4A74; }
        QPushButton, QToolButton { background: #26374F; border: 0; border-radius: 5px; color: #FFFFFF; padding: 7px 10px; }
        QPushButton:hover, QToolButton:hover { background: #466690; }
        QPushButton:checked, QToolButton:checked { background: #FF8A3D; }
        QHeaderView::section { background: #203457; color: #FFFFFF; border: 0; padding: 6px; }
        QTableWidget { gridline-color: #2E4568; alternate-background-color: #0F1A2D; }
        QCheckBox { spacing: 6px; }
        QSplitter::handle { background: #0E1624; border: 1px solid #243A5B; border-radius: 4px; }
        QSplitter::handle:hover { border-color: #6FA8FF; }
        QSplitter::handle:horizontal { margin: 2px 1px; }
        QSplitter::handle:vertical { margin: 1px 2px; }
    )");
}

void SessionWidget::refreshDevices()
{
    QString error;
    devices_ = ControlCanNative::instance().findDevices(&error);
    deviceCombo_->clear();
    for (const auto& device : devices_) {
        deviceCombo_->addItem(device.displayName(), device.deviceIndex);
    }
    if (!error.isEmpty()) {
        appendLog(QString("Device discovery failed: %1").arg(error));
    } else {
        appendLog(QString("Device list refreshed. Found %1 device(s).").arg(devices_.size()));
    }
}

void SessionWidget::connectDevice()
{
    try {
        if (deviceCombo_->currentIndex() < 0) {
            throw std::runtime_error("Select a device first.");
        }
        const quint32 deviceIndex = deviceCombo_->currentData().toUInt();
        const quint32 channelIndex = channelCombo_->currentText().toUInt();
        const QString baudRate = baudCombo_->currentText();
        const bool listenOnly = modeCombo_->currentText() == "Listen Only";
        const bool singleFilter = filterCombo_->currentText() == "Single Filter";

        service_.connectDevice(deviceIndex, channelIndex, baudRate, listenOnly, singleFilter);
        emit headerChanged(QString("Dev %1 / CH %2").arg(deviceIndex).arg(channelIndex));
        updateConnectionState();
    } catch (const std::exception& ex) {
        appendLog(QString("Connect failed: %1").arg(ex.what()));
        QMessageBox::critical(this, "Connect Error", ex.what());
    }
}

void SessionWidget::disconnectDevice()
{
    service_.disconnectDevice();
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
}

void SessionWidget::clearRxFrames()
{
    allFrames_.clear();
    groupedFrames_.clear();
    groupedIndex_.clear();
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
    rxFilterType_->setCurrentText("ALL");
    refreshRxTable();
}

void SessionWidget::exportTxCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Transmit Frames", "tx_frames.csv", "CSV files (*.csv);;All files (*.*)");
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
    const QString path = QFileDialog::getOpenFileName(this, "Import Transmit Frames", QString(), "CSV files (*.csv);;All files (*.*)");
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
            if (frame.cycleEnabled && service_.isConnected()) {
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
    const QString path = QFileDialog::getSaveFileName(this, "Export Receive Frames", "rx_frames.csv", "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(QString("Export RX CSV failed: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "Timestamp,Channel,ID,Type,Length,Data\n";
    for (const CanFrame& frame : activeRxFrames()) {
        if (!frameMatchesFilters(frame)) {
            continue;
        }
        out << csv(frame.timestamp) << ','
            << csv(frame.channel) << ','
            << csv(frame.idText) << ','
            << csv(frame.frameType) << ','
            << frame.length << ','
            << csv(frame.dataText) << '\n';
    }
    appendLog(QString("RX CSV exported: %1").arg(path));
}

void SessionWidget::exportLogCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export Event Log", "event_log.csv", "CSV files (*.csv);;All files (*.*)");
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

void SessionWidget::onFrameReceived(const CanFrame& frame)
{
    allFrames_.prepend(frame);
    while (allFrames_.size() > 500) {
        allFrames_.removeLast();
    }
    upsertGroupedFrame(frame);
    refreshRxTable();
    updateWatchValue(frame);
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
}

void SessionWidget::appendLog(const QString& message)
{
    logText_->append(message);
}

void SessionWidget::addTxRow(const TxFrame& frame)
{
    const int row = txTable_->rowCount();
    txTable_->insertRow(row);

    auto* actionWidget = new QWidget(txTable_);
    auto* actionLayout = new QHBoxLayout(actionWidget);
    actionLayout->setContentsMargins(2, 2, 2, 2);
    auto* manualButton = new QToolButton(actionWidget);
    auto* cycleButton = new QToolButton(actionWidget);
    cycleButton->setObjectName("cycleButton");
    manualButton->setText("M");
    cycleButton->setText("C");
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
            startCycleLoop(cycleButton);
        } else {
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

void SessionWidget::startCycleLoop(QToolButton* button)
{
    if (!button || !button->isChecked() || !service_.isConnected() || cycleTimers_.contains(button)) {
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

void SessionWidget::upsertGroupedFrame(const CanFrame& frame)
{
    const QString key = groupedKey(frame);
    if (groupedIndex_.contains(key)) {
        CanFrame& existing = groupedFrames_[groupedIndex_[key]];
        const int seen = existing.seenCount + 1;
        existing = frame;
        existing.seenCount = seen;
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

bool SessionWidget::frameMatchesFilters(const CanFrame& frame) const
{
    const QString idFilter = rxFilterId_->text().trimmed();
    if (!idFilter.isEmpty() && !frame.idText.contains(idFilter, Qt::CaseInsensitive)) {
        return false;
    }

    const QString type = rxFilterType_->currentText();
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
    rxTable_->setRowCount(0);
    for (const CanFrame& frame : activeRxFrames()) {
        if (!frameMatchesFilters(frame)) {
            continue;
        }
        const int row = rxTable_->rowCount();
        rxTable_->insertRow(row);
        const QStringList values = {
            frame.timestamp,
            QString::number(frame.seenCount),
            frame.channel,
            frame.idText,
            frame.frameType,
            QString::number(frame.length),
            frame.dataText
        };
        for (int column = 0; column < values.size(); ++column) {
            rxTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
}

QVector<CanFrame> SessionWidget::activeRxFrames() const
{
    return groupFramesCheck_->isChecked() ? groupedFrames_ : allFrames_;
}

void SessionWidget::updateWatchValue(const CanFrame& frame)
{
    try {
        quint32 watchId = 0;
        int startByte = 0;
        int length = 1;
        bool littleEndian = true;
        bool isSigned = false;
        if (!tryReadWatchConfig(watchId, startByte, length, littleEndian, isSigned)) {
            return;
        }
        if (frame.rawId != watchId) {
            return;
        }
        if (startByte < 0 || length <= 0 || startByte + length > frame.dataBytes.size()) {
            watchRawValue_->setText("Out of range");
            watchComputedValue_->setText("-");
            return;
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
    } catch (const std::exception& ex) {
        watchComputedValue_->setText(QString("Err: %1").arg(ex.what()));
    }
}

bool SessionWidget::tryReadWatchConfig(quint32& watchId, int& startByte, int& length, bool& littleEndian, bool& isSigned) const
{
    try {
        watchId = UsbCanService::parseCanId(watchId_->text());
        startByte = watchStartByte_->text().toInt();
        length = watchLength_->currentText().toInt();
        littleEndian = watchEndian_->currentText() == "Little";
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
