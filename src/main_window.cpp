#include "main_window.h"

#include "session_widget.h"

#include <QTabWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("ZVRNK CAN Tool");
    resize(1320, 820);
    setMinimumSize(0, 0);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    setCentralWidget(tabs_);

    tabs_->addTab(new QWidget(tabs_), "+ Add Session");
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::handleCurrentTabChanged);

    addSessionTab();
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
    const int insertIndex = tabs_->count() - 1;
    tabs_->insertTab(insertIndex, session, QString("Session %1").arg(tabId));
    tabs_->setCurrentIndex(insertIndex);

    connect(session, &SessionWidget::headerChanged, this, [this, session, tabId](const QString& status) {
        const int index = tabs_->indexOf(session);
        if (index >= 0) {
            tabs_->setTabText(index, QString("Session %1 - %2").arg(tabId).arg(status));
        }
    });
    connect(session, &SessionWidget::closeRequested, this, [this, session] {
        closeSessionTab(session);
    });

    addingTab_ = false;
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
