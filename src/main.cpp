#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("Robin");
    a.setOrganizationDomain("github.com/wming126/flash_helper");
    a.setApplicationName("FlashHelper");

    MainWindow w;
    w.show();
    return a.exec();
}
