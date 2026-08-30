QT       += core gui widgets network

TARGET    = mainpage
TEMPLATE  = app

SOURCES  += main.cpp \
            mainpage.cpp \
            backlightpanel.cpp \
            touchgesturedetector.cpp \
            settingsmanager.cpp \
            networkmanager.cpp \
            ../network/networkkeyboard.cpp

HEADERS  += mainpage.h \
            backlightpanel.h \
            touchgesturedetector.h \
            settingsmanager.h \
            networkmanager.h \
            ../network/networkkeyboard.h

INCLUDEPATH += ../network