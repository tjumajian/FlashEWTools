#include "flashwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    FlashWindow w;
    w.showMaximized();
    return a.exec();
}
