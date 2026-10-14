#include "theme.h"

#include <QApplication>
#include <QStyle>

namespace
{
QColor color(const char* value)
{
    return QColor(QString::fromLatin1(value));
}

QString cssColor(const QColor& value)
{
    return value.name(QColor::HexArgb);
}

void setCommonPalette(QPalette& palette, const ThemeTokens& tokens)
{
    palette.setColor(QPalette::Window, tokens.page);
    palette.setColor(QPalette::Base, tokens.inset);
    palette.setColor(QPalette::AlternateBase, tokens.panel);
    palette.setColor(QPalette::Button, tokens.panelRaised);
    palette.setColor(QPalette::Text, tokens.text);
    palette.setColor(QPalette::WindowText, tokens.text);
    palette.setColor(QPalette::ButtonText, tokens.text);
    palette.setColor(QPalette::PlaceholderText, tokens.textSubtle);
    palette.setColor(QPalette::Highlight, tokens.selection);
    palette.setColor(QPalette::HighlightedText, tokens.text);
    palette.setColor(QPalette::Accent, tokens.accent);
    palette.setColor(QPalette::Link, tokens.accent);
    palette.setColor(QPalette::LinkVisited, tokens.accent);
    palette.setColor(QPalette::BrightText, tokens.accentOn);
    palette.setColor(QPalette::ToolTipBase, tokens.panelRaised);
    palette.setColor(QPalette::ToolTipText, tokens.text);
    palette.setColor(QPalette::Light, tokens.panelRaised);
    palette.setColor(QPalette::Midlight, tokens.border);
    palette.setColor(QPalette::Mid, tokens.borderStrong);
    palette.setColor(QPalette::Dark, tokens.border);
    palette.setColor(QPalette::Shadow, tokens.page);
    palette.setColor(QPalette::Disabled, QPalette::Text, tokens.textSubtle);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, tokens.textSubtle);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, tokens.textSubtle);
}
}

ThemeTokens ThemeTokens::light()
{
    ThemeTokens tokens;
    tokens.page = color("#eef1f0");
    tokens.header = color("#ffffff");
    tokens.headerText = color("#132228");
    tokens.panel = color("#ffffff");
    tokens.panelRaised = color("#f6f8f8");
    tokens.inset = color("#f6f8f8");
    tokens.text = color("#132228");
    tokens.textMuted = color("#5a6a70");
    tokens.textSubtle = color("#6b777c");
    tokens.border = color("#d6dddd");
    tokens.borderStrong = color("#aebbbe");
    tokens.accent = color("#0f6b7d");
    tokens.accentHover = color("#0b5666");
    tokens.accentPressed = color("#084652");
    tokens.accentOn = color("#ffffff");
    tokens.selection = QColor(15, 107, 125, 40);
    tokens.focusRing = color("#0f6b7d");
    tokens.success = color("#1d7a4a");
    tokens.successSurface = color("#e2f2e8");
    tokens.warning = color("#b45309");
    tokens.warningSurface = color("#fdf0dc");
    tokens.danger = color("#b42318");
    tokens.dangerSurface = color("#fbe5e2");
    tokens.inactive = color("#6b777c");
    tokens.inactiveSurface = color("#e9eded");
    tokens.logSurface = color("#17262c");
    tokens.logText = color("#f2f5f6");
    return tokens;
}

ThemeTokens ThemeTokens::dark()
{
    ThemeTokens tokens;
    tokens.page = color("#0b1316");
    tokens.header = color("#0b1316");
    tokens.headerText = color("#e3ecee");
    tokens.panel = color("#121d22");
    tokens.panelRaised = color("#172429");
    tokens.inset = color("#172429");
    tokens.text = color("#e3ecee");
    tokens.textMuted = color("#93a6ad");
    tokens.textSubtle = color("#8b9aa0");
    tokens.border = color("#24353c");
    tokens.borderStrong = color("#3b535b");
    tokens.accent = color("#4fb8cc");
    tokens.accentHover = color("#6ac5d5");
    tokens.accentPressed = color("#2f97ad");
    tokens.accentOn = color("#062027");
    tokens.selection = QColor(79, 184, 204, 48);
    tokens.focusRing = color("#4fb8cc");
    tokens.success = color("#5fd08f");
    tokens.successSurface = color("#143224");
    tokens.warning = color("#f2b14c");
    tokens.warningSurface = color("#3a2a12");
    tokens.danger = color("#ff7a70");
    tokens.dangerSurface = color("#3d1916");
    tokens.inactive = color("#8b9aa0");
    tokens.inactiveSurface = color("#1d2a2f");
    tokens.logSurface = color("#0a1215");
    tokens.logText = color("#e3ecee");
    return tokens;
}

ThemeTokens ThemeTokens::forMode(ThemeMode mode)
{
    if (mode == ThemeMode::Light) {
        return light();
    }
    if (mode == ThemeMode::Dark) {
        return dark();
    }
    const QColor systemWindow = QApplication::style()->standardPalette().color(QPalette::Window);
    return systemWindow.lightness() >= 128 ? light() : dark();
}

QPalette ThemeTokens::palette() const
{
    QPalette result;
    setCommonPalette(result, *this);
    return result;
}

QString ThemeTokens::styleSheet() const
{
    QString result = QStringLiteral(R"(
        * { outline: none; }
        QWidget { background: @PAGE@; color: @TEXT@; font-family: "Segoe UI", "Noto Sans", sans-serif; font-size: 10pt; selection-background-color: @SELECTION@; selection-color: @TEXT@; }
        QMainWindow, QDialog, QTabWidget::pane, QMenu, QMenuBar, QStatusBar { background: @PAGE@; }
        QMenuBar#appMenuBar {
            background: @HEADER@;
            color: @HEADERTEXT@;
            border-bottom: 1px solid @BORDER@;
            padding: 2px 4px;
        }
        QMenuBar#appMenuBar::item {
            background: transparent;
            color: @HEADERTEXT@;
            padding: 4px 8px;
        }
        QMenuBar#appMenuBar::item:selected { background: @SELECTION@; color: @HEADERTEXT@; }
        QMenuBar#appMenuBar QToolButton#languageButton {
            background: transparent;
            border: 1px solid @HEADERTEXT@;
            border-radius: 0;
            color: @HEADERTEXT@;
            padding: 3px 8px;
            min-height: 18px;
            font-size: 9pt;
        }
        QMenuBar#appMenuBar QToolButton#languageButton:hover,
        QMenuBar#appMenuBar QToolButton#languageButton:pressed,
        QMenuBar#appMenuBar QToolButton#languageButton:focus {
            background: @SELECTION@;
            border-color: @ACCENT@;
        }
        QGroupBox { background: @PANEL@; border: 1px solid @BORDER@; border-radius: 0; margin-top: 14px; padding: 12px; font-weight: 600; }
        QGroupBox#headerPanel { background: @HEADER@; border-color: @BORDER@; color: @HEADERTEXT@; }
        QGroupBox#headerPanel QLabel#titleLabel { color: @HEADERTEXT@; font-family: "Bahnschrift", "DIN Alternate", "Segoe UI", sans-serif; font-size: 16pt; font-weight: 700; }
        QGroupBox#headerPanel QLabel#eyebrowLabel { color: @TEXTMUTED@; font-size: 8pt; font-weight: 700; letter-spacing: 1px; }
        QGroupBox#headerPanel QLabel#subtitleLabel { color: @TEXTMUTED@; font-size: 9pt; }
        QGroupBox#headerPanel QLabel#connectionStatus { color: @HEADERTEXT@; font-weight: 700; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; color: @TEXT@; background: @PANEL@; }
        QLabel { background: transparent; color: @TEXTMUTED@; }
        QLabel[state="success"] { color: @SUCCESS@; background: @SUCCESSSURFACE@; border: 1px solid @SUCCESS@; border-radius: 0; padding: 2px 7px; }
        QLabel[state="warn"] { color: @WARNING@; background: @WARNINGSURFACE@; border: 1px solid @WARNING@; border-radius: 0; padding: 2px 7px; }
        QLabel[state="danger"] { color: @DANGER@; background: @DANGERSURFACE@; border: 1px solid @DANGER@; border-radius: 0; padding: 2px 7px; }
        QLabel[state="inactive"] { color: @INACTIVE@; background: @INACTIVE_SURFACE@; border: 1px solid @BORDERSTRONG@; border-radius: 0; padding: 2px 7px; }
        QLineEdit, QComboBox, QTextEdit { background: @INSET@; border: 1px solid @BORDERSTRONG@; border-radius: 0; color: @TEXT@; padding: 6px 8px; }
        QLineEdit:hover, QComboBox:hover, QTextEdit:hover { border-color: @ACCENT@; }
        QLineEdit:focus, QComboBox:focus, QTextEdit:focus { border: 2px solid @FOCUS@; padding: 5px 7px; background: @PANELRAISED@; }
        QTableWidget:focus, QTableView:focus, QPushButton:focus, QToolButton:focus, QCheckBox:focus { border: 2px solid @FOCUS@; }
        QLineEdit:disabled, QComboBox:disabled, QTextEdit:disabled { background: @INACTIVE_SURFACE@; color: @TEXTSUBTLE@; border-color: @BORDER@; }
        QComboBox { border-radius: 0; }
        QComboBox::drop-down { width: 24px; border: 0; border-left: 1px solid @BORDER@; border-top-right-radius: 0; border-bottom-right-radius: 0; }
        QComboBox QAbstractItemView { background: @PANELRAISED@; border: 1px solid @BORDERSTRONG@; color: @TEXT@; selection-background-color: @SELECTION@; outline: 0; }

        /* Keep the receive-side panel dense without changing the transmit/header hierarchy. */
        QGroupBox#receivedFramesPanel {
            margin-top: 12px;
            padding: 8px;
        }
        QGroupBox#receivedFramesPanel QLabel {
            font-size: 9pt;
        }
        QGroupBox#receivedFramesPanel QPushButton,
        QGroupBox#receivedFramesPanel QToolButton {
            padding: 4px 8px;
            margin: 0;
            min-height: 18px;
            font-size: 9pt;
        }
        QGroupBox#receivedFramesPanel QLineEdit,
        QGroupBox#receivedFramesPanel QComboBox {
            padding: 4px 6px;
            min-height: 18px;
            font-size: 9pt;
        }
        QGroupBox#receivedFramesPanel QCheckBox {
            spacing: 5px;
            padding: 2px;
            font-size: 9pt;
        }
        QGroupBox#receivedFramesPanel QComboBox::drop-down {
            width: 19px;
        }

        QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
            width: 0;
            height: 0;
            background: transparent;
            border: 0;
        }
        QAbstractSpinBox::up-arrow, QAbstractSpinBox::down-arrow {
            image: none;
            width: 0;
            height: 0;
        }

        QCheckBox { spacing: 7px; color: @TEXTMUTED@; background: transparent; }
        QCheckBox::indicator { width: 15px; height: 15px; border-radius: 0; border: 1px solid @BORDERSTRONG@; background: @INSET@; }
        QCheckBox::indicator:hover { border-color: @ACCENT@; }
        QCheckBox::indicator:checked { background: @ACCENT@; border-color: @ACCENT@; }
        QPushButton, QToolButton { background: transparent; border: 1px solid @BORDERSTRONG@; border-radius: 0; color: @TEXT@; padding: 4px 8px; min-height: 20px; font-size: 9pt; font-weight: 600; }
        QPushButton:hover, QToolButton:hover { background: @INSET@; border-color: @ACCENT@; }
        QPushButton:pressed, QToolButton:pressed { background: @SELECTION@; border-color: @ACCENT@; }
        QPushButton:disabled, QToolButton:disabled { background: @INACTIVE_SURFACE@; border-color: @BORDER@; color: @TEXTSUBTLE@; }
        QPushButton[variant="primary"], QPushButton#primaryButton { background: transparent; border-color: @TEXT@; color: @TEXT@; }
        QPushButton[variant="primary"]:hover, QPushButton#primaryButton:hover { background: @INSET@; border-color: @ACCENT@; }
        QPushButton[variant="primary"]:pressed, QPushButton#primaryButton:pressed { background: @TEXT@; border-color: @TEXT@; color: @PANEL@; }
        QPushButton[variant="ghost"] { background: transparent; border-color: transparent; color: @TEXTMUTED@; }
        QPushButton[variant="ghost"]:hover { background: @INSET@; border-color: @BORDER@; }
        QPushButton[tone="warn"], QPushButton#warningButton { background: transparent; border-color: @WARNING@; color: @WARNING@; }
        QPushButton[tone="warn"]:hover, QPushButton#warningButton:hover { background: @WARNINGSURFACE@; }
        QPushButton[tone="danger"], QPushButton#dangerButton { background: transparent; border-color: @DANGER@; color: @DANGER@; }
        QPushButton[tone="danger"]:hover, QPushButton#dangerButton:hover { background: @DANGERSURFACE@; }
        QPushButton#secondaryButton { background: transparent; border-color: @BORDERSTRONG@; color: @TEXTMUTED@; }
        QPushButton#toggleButton:checked, QToolButton#cycleButton:checked { background: @SELECTION@; border-color: @ACCENT@; color: @TEXT@; }
        QTableWidget, QTableView { background: @INSET@; alternate-background-color: @PANEL@; border: 1px solid @BORDER@; border-radius: 0; color: @TEXT@; gridline-color: @BORDER@; selection-background-color: @SELECTION@; selection-color: @TEXT@; }
        QTableWidget::item, QTableView::item { padding: 4px; border: 0; }
        QTableWidget::item:hover, QTableView::item:hover { background: @PANELRAISED@; }
        QHeaderView::section { background: @PANELRAISED@; color: @TEXT@; border: 0; border-right: 1px solid @BORDER@; border-bottom: 1px solid @BORDERSTRONG@; padding: 7px 8px; font-weight: 600; }
        QTextEdit { font-family: Consolas, "Cascadia Mono", monospace; background: @LOGSURFACE@; color: @LOGTEXT@; }
        QSplitter::handle { background: @PAGE@; border: 0; border-radius: 0; }
        QSplitter::handle:horizontal { width: 5px; border-left: 1px solid @BORDER@; }
        QSplitter::handle:vertical { height: 5px; border-top: 1px solid @BORDER@; }
        QSplitter::handle:hover { background: @PANEL@; border-color: @ACCENT@; }
        QScrollBar:vertical, QScrollBar:horizontal { background: @PAGE@; border: 0; width: 11px; height: 11px; margin: 0; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: @BORDERSTRONG@; border-radius: 0; min-height: 28px; min-width: 28px; }
        QScrollBar::handle:hover { background: @ACCENT@; }
        QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: transparent; border: 0; }
        QTabWidget::pane { border: 0; }
        QTabBar::tab { background: @PANEL@; color: @TEXTMUTED@; border: 1px solid @BORDER@; border-bottom: 0; padding: 8px 14px; margin-right: 2px; min-width: 110px; }
        QTabBar::tab:selected { background: @PANELRAISED@; color: @TEXT@; border-color: @ACCENT@; }
        QTabBar::tab:hover:!selected { background: @INSET@; color: @TEXT@; }
        QMenuBar { color: @TEXTMUTED@; }
        QMenuBar::item:selected, QMenu::item:selected { background: @SELECTION@; color: @TEXT@; }
        QMenu { border: 1px solid @BORDERSTRONG@; color: @TEXT@; }
        /* Header chrome stays neutral; semantic colors belong to content/status surfaces. */
        QGroupBox#headerPanel QLabel,
        QGroupBox#headerPanel QPushButton,
        QGroupBox#headerPanel QPushButton:hover,
        QGroupBox#headerPanel QPushButton:pressed,
        QGroupBox#headerPanel QPushButton:disabled,
        QGroupBox#headerPanel QComboBox,
        QGroupBox#headerPanel QComboBox:hover,
        QGroupBox#headerPanel QCheckBox {
            color: @HEADERTEXT@;
        }
        QGroupBox#headerPanel QLabel[state] {
            color: @HEADERTEXT@;
            background: transparent;
            border-color: @HEADERTEXT@;
        }
        QGroupBox#headerPanel QPushButton,
        QGroupBox#headerPanel QPushButton:hover,
        QGroupBox#headerPanel QPushButton:pressed,
        QGroupBox#headerPanel QPushButton:disabled,
        QGroupBox#headerPanel QComboBox,
        QGroupBox#headerPanel QComboBox:hover,
        QGroupBox#headerPanel QCheckBox {
            background: transparent;
            border-color: @HEADERTEXT@;
        }
        QGroupBox#headerPanel QPushButton:hover,
        QGroupBox#headerPanel QPushButton:pressed,
        QGroupBox#headerPanel QComboBox:hover {
            background: @SELECTION@;
            border-color: @ACCENT@;
        }
        QTabWidget::pane { border: 0; }
        /* The session strip is the header's inner top edge, not a separate card. */
        QTabWidget#sessionTabs { background: @HEADER@; }
        QTabWidget#sessionTabs::pane { background: @PAGE@; border: 0; }
        QTabWidget#sessionTabs QTabBar#sessionTabBar {
            background: @HEADER@;
            border: 0;
            border-bottom: 1px solid @BORDER@;
            padding: 3px 12px 0;
        }
        QTabWidget#sessionTabs QTabBar#sessionTabBar::tab {
            background: transparent;
            color: @TEXTMUTED@;
            border: 0;
            border-bottom: 2px solid transparent;
            padding: 4px 9px 4px;
            margin-right: 2px;
            min-width: 0;
            font-size: 9pt;
        }
        QTabWidget#sessionTabs QTabBar#sessionTabBar::tab:selected {
            color: @HEADERTEXT@;
            border-bottom-color: @ACCENT@;
        }
        QTabWidget#sessionTabs QTabBar#sessionTabBar::tab:hover:!selected {
            color: @HEADERTEXT@;
            background: @SELECTION@;
        }
        QTabWidget#sessionTabs QGroupBox#headerPanel {
            margin-top: 0;
            border-top: 0;
        }
        QTabBar::tab {
            background: transparent;
            color: @TEXTMUTED@;
            border: 0;
            border-bottom: 2px solid transparent;
            padding: 5px 9px 4px;
            margin-right: 2px;
            min-width: 0;
            font-size: 9pt;
        }
        QTabBar::tab:selected { color: @TEXT@; border-bottom-color: @ACCENT@; }
        QTabBar::tab:hover:!selected { color: @TEXT@; background: @INSET@; }
        QStatusBar { color: @TEXTSUBTLE@; }
    )");

    const auto replace = [&result](const QString& token, const QColor& value) {
        result.replace(token, cssColor(value));
    };
    replace("@PAGE@", page); replace("@HEADER@", header); replace("@HEADERTEXT@", headerText);
    replace("@PANEL@", panel); replace("@PANELRAISED@", panelRaised);
    replace("@INSET@", inset); replace("@TEXT@", text); replace("@TEXTMUTED@", textMuted);
    replace("@TEXTSUBTLE@", textSubtle); replace("@BORDER@", border); replace("@BORDERSTRONG@", borderStrong);
    replace("@ACCENT@", accent); replace("@ACCENTHOVER@", accentHover); replace("@ACCENTON@", accentOn);
    replace("@SELECTION@", selection); replace("@FOCUS@", focusRing); replace("@SUCCESS@", success);
    replace("@SUCCESSSURFACE@", successSurface); replace("@WARNING@", warning); replace("@WARNINGSURFACE@", warningSurface);
    replace("@DANGER@", danger); replace("@DANGERSURFACE@", dangerSurface); replace("@INACTIVE_SURFACE@", inactiveSurface);
    replace("@INACTIVE@", inactive);
    replace("@LOGSURFACE@", logSurface); replace("@LOGTEXT@", logText);
    return result;
}
