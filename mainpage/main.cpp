#include <QApplication>
#include <QDebug>
#include <cstdio>
#include "mainpage.h"

int main(int argc, char *argv[])
{
    printf("DEBUG: mainpage started\n");
    fflush(stdout);

    QApplication app(argc, argv);
    printf("DEBUG: QApplication created, platform=%s\n",
           app.platformName().toLocal8Bit().constData());
    fflush(stdout);

    MainPage w;
    w.show();
    printf("DEBUG: mainpage shown\n");
    fflush(stdout);

    return app.exec();
}