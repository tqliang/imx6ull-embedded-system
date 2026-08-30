#include <QApplication>
#include <QDebug>
#include <cstdio>
#include "terminalwidget.h"

int main(int argc, char *argv[])
{
    printf("DEBUG: terminal started\n");
    fflush(stdout);

    QApplication app(argc, argv);
    printf("DEBUG: QApplication created, platform=%s\n",
           app.platformName().toLocal8Bit().constData());
    fflush(stdout);

    TerminalWidget w;
    w.show();
    printf("DEBUG: terminal shown\n");
    fflush(stdout);

    return app.exec();
}