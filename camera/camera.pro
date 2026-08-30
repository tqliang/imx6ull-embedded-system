QT       += core gui widgets

TARGET    = camera
TEMPLATE  = app

SOURCES  += main.cpp \
            camerawidget.cpp \
            v4l2capture.cpp \
            mjpegrecorder.cpp \
            gallerydialog.cpp

HEADERS  += camerawidget.h \
            v4l2capture.h \
            mjpegrecorder.h \
            gallerydialog.h

# NEON SIMD acceleration for YUYV->RGB conversion
QMAKE_CXXFLAGS += -mfpu=neon