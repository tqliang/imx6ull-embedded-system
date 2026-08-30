QT       += core gui widgets

TARGET    = network
TEMPLATE  = app

SOURCES  += main.cpp \
            networksettingswidget.cpp \
            networkkeyboard.cpp \
            ../mainpage/settingsmanager.cpp

HEADERS  += networksettingswidget.h \
            networkkeyboard.h \
            ../mainpage/settingsmanager.h

INCLUDEPATH += ../mainpage