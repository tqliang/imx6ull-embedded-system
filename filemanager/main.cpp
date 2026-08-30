#include <QApplication>
#include <QDebug>
#include <cstdio>
#include "filemanagerwidget.h"

int main(int argc, char *argv[])
{
    printf("DEBUG: filemanager started\n");
    fflush(stdout);

    QApplication app(argc, argv);
    printf("DEBUG: QApplication created, platform=%s\n",
           app.platformName().toLocal8Bit().constData());
    fflush(stdout);

    FileManagerWidget w;
    w.show();
    printf("DEBUG: filemanager shown\n");
    fflush(stdout);

    return app.exec();
}