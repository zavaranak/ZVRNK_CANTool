#pragma once

#include "localization.h"
#include "theme.h"

#include <QMainWindow>

class QTabWidget;
class SessionWidget;
class QAction;
class QMenu;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void handleCurrentTabChanged(int index);

private:
    void addSessionTab();
    void closeSessionTab(SessionWidget* session);
    void setThemeMode(ThemeMode mode);
    void setLanguage(AppLanguage language);
    void retranslateUi();
    void updateLanguageActions();

    QTabWidget* tabs_ = nullptr;
    int tabCounter_ = 0;
    bool addingTab_ = false;
    ThemeMode themeMode_ = ThemeMode::System;
    ThemeTokens theme_ = ThemeTokens::dark();
    QAction* systemThemeAction_ = nullptr;
    QAction* lightThemeAction_ = nullptr;
    QAction* darkThemeAction_ = nullptr;
    QAction* englishLanguageAction_ = nullptr;
    QAction* vietnameseLanguageAction_ = nullptr;
    QMenu* themeMenu_ = nullptr;
    QMenu* languageMenu_ = nullptr;
    AppLanguage language_ = AppLanguage::English;
};
