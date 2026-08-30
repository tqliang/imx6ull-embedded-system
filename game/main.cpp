#include <QApplication>
#include <QDebug>
#include <cstdio>
#include "spaceship.h"

int main(int argc, char *argv[])
{
    printf("DEBUG: spaceship game started\n");
    fflush(stdout);

    QApplication app(argc, argv);
    printf("DEBUG: QApplication created, platform=%s\n",
           app.platformName().toLocal8Bit().constData());
    fflush(stdout);

    ShipGame w;
    w.showFullScreen();
    printf("DEBUG: game shown\n");
    fflush(stdout);

    return app.exec();
}