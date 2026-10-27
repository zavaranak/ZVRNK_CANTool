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
    // Draw the whole UI at 80% of the system scale; fonts, padding and fixed
    // sizes shrink together. An explicit QT_SCALE_FACTOR still wins.
    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        qputenv("QT_SCALE_FACTOR", "0.8");
    }
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ZVRNK");
    QCoreApplication::setApplicationName("ZVRNK CAN Tool");
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
