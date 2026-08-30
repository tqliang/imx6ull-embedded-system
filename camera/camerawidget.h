#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QPixmap>
#include "v4l2capture.h"
#include "mjpegrecorder.h"

class CameraWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraWidget(QWidget *parent = nullptr);
    ~CameraWidget();

private slots:
    void onCapture();
    void onStartStop();
    void onPhoto();
    void onRecord();
    void onGallery();
    void onBrightnessChanged(int value);
    void onContrastChanged(int value);
    void onSaturationChanged(int value);

private:
    void applyControl(uint32_t id, int value);

    V4l2Capture   *m_capture;
    MjpegRecorder *m_recorder;
    QTimer        *m_timer;
    QLabel        *m_label;
    QLabel        *m_fpsLabel;
    QLabel        *m_recLabel;
    QPushButton   *m_btn;
    QPushButton   *m_photoBtn;
    QPushButton   *m_recordBtn;
    QPushButton   *m_galleryBtn;
    QPushButton   *m_exitBtn;
    QSlider       *m_brightnessSlider;
    QSlider       *m_contrastSlider;
    QSlider       *m_saturationSlider;
    QLabel        *m_brightnessLabel;
    QLabel        *m_contrastLabel;
    QLabel        *m_saturationLabel;
    QPushButton   *m_brightnessMinus;
    QPushButton   *m_brightnessPlus;
    QPushButton   *m_contrastMinus;
    QPushButton   *m_contrastPlus;
    QPushButton   *m_saturationMinus;
    QPushButton   *m_saturationPlus;
    QElapsedTimer  m_fpsTimer;
    QElapsedTimer  m_recTimer;
    QPixmap        m_pixmap;
    int            m_frameCount;
    bool           m_active;
};

#endif