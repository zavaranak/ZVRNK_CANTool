#include "can_types.h"
#include "main_window.h"

#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QStyle>
#include <QStyleFactory>
#include <QVector>

int main(int argc, char* argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/logo.png"));
    if (QStyle* fusionStyle = QStyleFactory::create("Fusion")) {
        app.setStyle(fusionStyle);
    }
    qRegisterMetaType<CanFrame>("CanFrame");
    qRegisterMetaType<QVector<CanFrame>>("QVector<CanFrame>");

    MainWindow window;
    window.setWindowIcon(app.windowIcon());
    window.adjustSize();
    window.show();

    return app.exec();
}
