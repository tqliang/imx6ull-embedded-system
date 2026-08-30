#include <QApplication>
#include <QDebug>
#include <cstdio>
#include "networksettingswidget.h"

int main(int argc, char *argv[])
{
    printf("DEBUG: network started\n");
    fflush(stdout);

    QApplication app(argc, argv);
    printf("DEBUG: QApplication created, platform=%s\n",
           app.platformName().toLocal8Bit().constData());
    fflush(stdout);

    NetworkSettingsWidget w;
    w.show();
    printf("DEBUG: network shown\n");
    fflush(stdout);

    return app.exec();
}