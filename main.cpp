#include "aeroWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AeroWindow w;
    w.showFullScreen();
    return a.exec();
}
