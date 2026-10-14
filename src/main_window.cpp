#include "main_window.h"

#include "session_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("ZVRNK CAN Tool");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    menuBar()->setNativeMenuBar(false);
    menuBar()->setObjectName("appMenuBar");

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("sessionTabs");
    tabs_->setDocumentMode(true);
    tabs_->tabBar()->setObjectName("sessionTabBar");
    tabs_->tabBar()->setDrawBase(false);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setElideMode(Qt::ElideRight);
    setCentralWidget(tabs_);

    themeMenu_ = menuBar()->addMenu("&View");
    themeMenu_->setObjectName("themeMenu");
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    systemThemeAction_ = themeMenu_->addAction("System theme");
    lightThemeAction_ = themeMenu_->addAction("Light theme");
    darkThemeAction_ = themeMenu_->addAction("Dark theme");
    for (QAction* action : {systemThemeAction_, lightThemeAction_, darkThemeAction_}) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }
    systemThemeAction_->setChecked(true);
    connect(systemThemeAction_, &QAction::triggered, this, [this] { setThemeMode(ThemeMode::System); });
    connect(lightThemeAction_, &QAction::triggered, this, [this] { setThemeMode(ThemeMode::Light); });
    connect(darkThemeAction_, &QAction::triggered, this, [this] { setThemeMode(ThemeMode::Dark); });

    languageMenu_ = new QMenu("Language", this);
    languageMenu_->setObjectName("languageMenu");
    languageMenu_->setAccessibleName("Language menu");
    auto* languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    englishLanguageAction_ = languageMenu_->addAction("English");
    englishLanguageAction_->setCheckable(true);
    languageGroup->addAction(englishLanguageAction_);
    vietnameseLanguageAction_ = languageMenu_->addAction("Vietnamese");
    vietnameseLanguageAction_->setCheckable(true);
    languageGroup->addAction(vietnameseLanguageAction_);
    connect(englishLanguageAction_, &QAction::triggered, this, [this] { setLanguage(AppLanguage::English); });
    connect(vietnameseLanguageAction_, &QAction::triggered, this, [this] { setLanguage(AppLanguage::Vietnamese); });

    auto* languageButton = new QToolButton(menuBar());
    languageButton->setObjectName("languageButton");
    languageButton->setText("Language");
    languageButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    languageButton->setPopupMode(QToolButton::InstantPopup);
    languageButton->setMenu(languageMenu_);
    languageButton->setAccessibleName("Language selector");
    languageButton->setToolTip("Choose English or Vietnamese");
    menuBar()->setCornerWidget(languageButton, Qt::TopRightCorner);
    setThemeMode(ThemeMode::System);

    const int addTabIndex = tabs_->addTab(new QWidget(tabs_), "+ Add Session");
    tabs_->tabBar()->setTabData(addTabIndex, "+ Add Session");
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::handleCurrentTabChanged);

    addSessionTab();
    setLanguage(AppLanguage::English);
}

void MainWindow::setThemeMode(ThemeMode mode)
{
    themeMode_ = mode;
    systemThemeAction_->setChecked(mode == ThemeMode::System);
    lightThemeAction_->setChecked(mode == ThemeMode::Light);
    darkThemeAction_->setChecked(mode == ThemeMode::Dark);
    theme_ = ThemeTokens::forMode(themeMode_);
    qApp->setPalette(theme_.palette());
    qApp->setStyleSheet(theme_.styleSheet());
}

MainWindow::~MainWindow()
{
    for (int i = 0; i < tabs_->count(); ++i) {
        if (auto* session = qobject_cast<SessionWidget*>(tabs_->widget(i))) {
            session->shutdown();
        }
    }
}

void MainWindow::handleCurrentTabChanged(int index)
{
    if (addingTab_ || index != tabs_->count() - 1) {
        return;
    }
    addSessionTab();
}

void MainWindow::addSessionTab()
{
    addingTab_ = true;
    ++tabCounter_;
    const int tabId = tabCounter_;
    auto* session = new SessionWidget(this);
    session->setLanguage(language_);
    const int insertIndex = tabs_->count() - 1;
    const QString tabSource = QString("Session %1").arg(tabId);
    tabs_->insertTab(insertIndex, session, localize(language_, tabSource));
    tabs_->tabBar()->setTabData(insertIndex, tabSource);
    tabs_->setCurrentIndex(insertIndex);

    connect(session, &SessionWidget::headerChanged, this, [this, session, tabId](const QString& status) {
        const int index = tabs_->indexOf(session);
        if (index >= 0) {
            const QString source = QString("Session %1 - %2").arg(tabId).arg(status);
            tabs_->tabBar()->setTabData(index, source);
            tabs_->setTabText(index, localize(language_, source));
        }
    });
    connect(session, &SessionWidget::closeRequested, this, [this, session] {
        closeSessionTab(session);
    });

    addingTab_ = false;
}

void MainWindow::setLanguage(AppLanguage language)
{
    language_ = language;
    for (SessionWidget* session : findChildren<SessionWidget*>()) {
        session->setLanguage(language_);
    }
    retranslateUi();
    updateLanguageActions();
}

void MainWindow::retranslateUi()
{
    themeMenu_->setTitle(localize(language_, "View"));
    systemThemeAction_->setText(localize(language_, "System theme"));
    lightThemeAction_->setText(localize(language_, "Light theme"));
    darkThemeAction_->setText(localize(language_, "Dark theme"));
    languageMenu_->setTitle(localize(language_, "Language"));
    languageMenu_->setAccessibleName(localize(language_, "Language menu"));
    englishLanguageAction_->setText(localize(language_, "English"));
    vietnameseLanguageAction_->setText(localize(language_, "Vietnamese"));
    if (auto* languageButton = findChild<QToolButton*>("languageButton")) {
        languageButton->setText(localize(language_, "Language"));
        languageButton->setAccessibleName(localize(language_, "Language selector"));
        languageButton->setToolTip(localize(language_, "Choose English or Vietnamese"));
    }
    for (int index = 0; index < tabs_->count(); ++index) {
        const QString source = tabs_->tabBar()->tabData(index).toString();
        if (!source.isEmpty()) {
            tabs_->setTabText(index, localize(language_, source));
        }
    }
    setWindowTitle(localize(language_, "ZVRNK CAN Tool"));
}

void MainWindow::updateLanguageActions()
{
    englishLanguageAction_->setChecked(language_ == AppLanguage::English);
    vietnameseLanguageAction_->setChecked(language_ == AppLanguage::Vietnamese);
}

void MainWindow::closeSessionTab(SessionWidget* session)
{
    const int index = tabs_->indexOf(session);
    if (index < 0) {
        return;
    }

    session->shutdown();
    tabs_->removeTab(index);
    session->deleteLater();

    if (tabs_->count() == 1) {
        addSessionTab();
    }
}
