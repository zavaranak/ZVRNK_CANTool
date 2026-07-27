#include "can_types.h"
#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    qRegisterMetaType<CanFrame>("CanFrame");

    MainWindow window;
    window.show();

    return app.exec();
}
