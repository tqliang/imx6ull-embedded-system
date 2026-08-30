QT       += core gui widgets

TARGET    = audio
TEMPLATE  = app

SOURCES  += main.cpp \
            audiowidget.cpp \
            spectrumwidget.cpp

HEADERS  += audiowidget.h \
            spectrumwidget.h

QMAKE_CXXFLAGS += -mfpu=neon
LIBS += -lasound -lmad