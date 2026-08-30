#include <QApplication>
#include <QDebug>
#include <cstdio>
#include "audiowidget.h"

int main(int argc, char *argv[])
{
    printf("DEBUG: audio main started\n");
    fflush(stdout);

    QApplication app(argc, argv);
    printf("DEBUG: QApplication created, platform=%s\n",
           app.platformName().toLocal8Bit().constData());
    fflush(stdout);

    AudioWidget w;
    w.show();
    printf("DEBUG: widget shown\n");
    fflush(stdout);

    return app.exec();
}