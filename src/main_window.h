#pragma once

#include <QMainWindow>

class QTabWidget;
class SessionWidget;

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

    QTabWidget* tabs_ = nullptr;
    int tabCounter_ = 0;
    bool addingTab_ = false;
};
