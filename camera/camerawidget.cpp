#include "camerawidget.h"
#include "gallerydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <sys/stat.h>
#include <linux/videodev2.h>

static const char *kSaveDir = "/home/user";

CameraWidget::CameraWidget(QWidget *parent)
    : QWidget(parent)
    , m_frameCount(0)
    , m_active(false)
{
    setWindowTitle("OV5640 Camera");

    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_label->setStyleSheet("background-color: #1a1a2e; color: #555;");

    QWidget *bottomBar = new QWidget(this);
    bottomBar->setStyleSheet("background: #141428;");

    QFont labelFont("WenQuanYi Zen Hei", 11, QFont::Bold);
    QFont btnFont("WenQuanYi Zen Hei", 14, QFont::Bold);

    m_fpsLabel = new QLabel("FPS: --", bottomBar);
    m_fpsLabel->setFont(QFont("WenQuanYi Zen Hei", 11, QFont::Bold));
    m_fpsLabel->setStyleSheet("color: #00e676; background: transparent; padding: 0 4px;");

    m_recLabel = new QLabel(bottomBar);
    m_recLabel->setFont(QFont("WenQuanYi Zen Hei", 11, QFont::Bold));
    m_recLabel->setStyleSheet("color: #ff1744; background: transparent; padding: 0 4px;");
    m_recLabel->hide();

    QHBoxLayout *statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(12, 6, 12, 2);
    statusRow->setSpacing(0);
    statusRow->addWidget(m_fpsLabel);
    statusRow->addStretch();
    statusRow->addWidget(m_recLabel);

    QString sliderStyle =
        "QSlider::groove:horizontal {"
        "  border: none; height: 4px;"
        "  background: rgba(255,255,255,25);"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #FFD54F;"
        "  width: 18px; height: 18px;"
        "  margin: -7px 0;"
        "  border-radius: 9px;"
        "}"
        "QSlider::handle:horizontal:disabled {"
        "  background: #555;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #FFD54F;"
        "  border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal:disabled {"
        "  background: #444;"
        "}";

    m_brightnessLabel = new QLabel("亮度", bottomBar);
    m_brightnessLabel->setFont(labelFont);
    m_brightnessLabel->setStyleSheet("color: #FFD54F; background: transparent;");
    m_brightnessLabel->setFixedWidth(36);
    m_brightnessLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_brightnessSlider = new QSlider(Qt::Horizontal, bottomBar);
    m_brightnessSlider->setRange(0, 255);
    m_brightnessSlider->setValue(64);
    m_brightnessSlider->setMinimumWidth(80);
    m_brightnessSlider->setStyleSheet(sliderStyle);

    m_contrastLabel = new QLabel("对比度", bottomBar);
    m_contrastLabel->setFont(labelFont);
    m_contrastLabel->setStyleSheet("color: #FFD54F; background: transparent;");
    m_contrastLabel->setFixedWidth(48);
    m_contrastLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_contrastSlider = new QSlider(Qt::Horizontal, bottomBar);
    m_contrastSlider->setRange(0, 255);
    m_contrastSlider->setValue(64);
    m_contrastSlider->setMinimumWidth(80);
    m_contrastSlider->setStyleSheet(sliderStyle);

    m_saturationLabel = new QLabel("饱和度", bottomBar);
    m_saturationLabel->setFont(labelFont);
    m_saturationLabel->setStyleSheet("color: #FFD54F; background: transparent;");
    m_saturationLabel->setFixedWidth(48);
    m_saturationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_saturationSlider = new QSlider(Qt::Horizontal, bottomBar);
    m_saturationSlider->setRange(0, 255);
    m_saturationSlider->setValue(64);
    m_saturationSlider->setMinimumWidth(80);
    m_saturationSlider->setStyleSheet(sliderStyle);
    m_brightnessSlider->setEnabled(false);
    m_contrastSlider->setEnabled(false);
    m_saturationSlider->setEnabled(false);

    QString stepBtnStyle =
        "QPushButton { background: #2a2a3e; color: #FFD54F; border: 1px solid #3a3a4e;"
        "  border-radius: 3px; font-size: 14px; font-weight: bold; }"
        "QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }"
        "QPushButton:disabled { background: #1a1a2e; color: #444; border-color: #222; }";

    auto makeStepBtn = [&](const QString &text) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, bottomBar);
        btn->setFixedSize(24, 24);
        btn->setFont(QFont("WenQuanYi Zen Hei", 14, QFont::Bold));
        btn->setStyleSheet(stepBtnStyle);
        btn->setEnabled(false);
        return btn;
    };

    m_brightnessMinus = makeStepBtn("-");
    m_brightnessPlus  = makeStepBtn("+");
    m_contrastMinus   = makeStepBtn("-");
    m_contrastPlus    = makeStepBtn("+");
    m_saturationMinus = makeStepBtn("-");
    m_saturationPlus  = makeStepBtn("+");

    connect(m_brightnessMinus, &QPushButton::clicked, this, [this]() {
        m_brightnessSlider->setValue(m_brightnessSlider->value() - 5);
    });
    connect(m_brightnessPlus, &QPushButton::clicked, this, [this]() {
        m_brightnessSlider->setValue(m_brightnessSlider->value() + 5);
    });
    connect(m_contrastMinus, &QPushButton::clicked, this, [this]() {
        m_contrastSlider->setValue(m_contrastSlider->value() - 5);
    });
    connect(m_contrastPlus, &QPushButton::clicked, this, [this]() {
        m_contrastSlider->setValue(m_contrastSlider->value() + 5);
    });
    connect(m_saturationMinus, &QPushButton::clicked, this, [this]() {
        m_saturationSlider->setValue(m_saturationSlider->value() - 5);
    });
    connect(m_saturationPlus, &QPushButton::clicked, this, [this]() {
        m_saturationSlider->setValue(m_saturationSlider->value() + 5);
    });

    QHBoxLayout *sliderRow = new QHBoxLayout;
    sliderRow->setContentsMargins(12, 2, 12, 4);
    sliderRow->setSpacing(2);
    sliderRow->addStretch();
    sliderRow->addWidget(m_brightnessLabel);
    sliderRow->addWidget(m_brightnessMinus);
    sliderRow->addWidget(m_brightnessSlider);
    sliderRow->addWidget(m_brightnessPlus);
    sliderRow->addSpacing(6);
    sliderRow->addWidget(m_contrastLabel);
    sliderRow->addWidget(m_contrastMinus);
    sliderRow->addWidget(m_contrastSlider);
    sliderRow->addWidget(m_contrastPlus);
    sliderRow->addSpacing(6);
    sliderRow->addWidget(m_saturationLabel);
    sliderRow->addWidget(m_saturationMinus);
    sliderRow->addWidget(m_saturationSlider);
    sliderRow->addWidget(m_saturationPlus);
    sliderRow->addStretch();

    m_btn = new QPushButton("开始", bottomBar);
    m_btn->setFixedSize(100, 42);
    m_btn->setFont(btnFont);
    m_btn->setStyleSheet("QPushButton { background: #555; color: white; border: none; border-radius: 6px; }"
                         "QPushButton:pressed { background: #333; }");

    m_photoBtn = new QPushButton("拍照", bottomBar);
    m_photoBtn->setFixedSize(100, 42);
    m_photoBtn->setFont(btnFont);
    m_photoBtn->setStyleSheet("QPushButton { background: #4CAF50; color: white; border: none; border-radius: 6px; }"
                              "QPushButton:pressed { background: #388E3C; }"
                              "QPushButton:disabled { background: #555; color: #777; }");
    m_photoBtn->setEnabled(false);

    m_recordBtn = new QPushButton("录像", bottomBar);
    m_recordBtn->setFixedSize(100, 42);
    m_recordBtn->setFont(btnFont);
    m_recordBtn->setStyleSheet("QPushButton { background: #f44336; color: white; border: none; border-radius: 6px; }"
                               "QPushButton:pressed { background: #c62828; }"
                               "QPushButton:disabled { background: #555; color: #777; }");
    m_recordBtn->setEnabled(false);

    m_galleryBtn = new QPushButton("相册", bottomBar);
    m_galleryBtn->setFixedSize(100, 42);
    m_galleryBtn->setFont(btnFont);
    m_galleryBtn->setStyleSheet("QPushButton { background: #2196F3; color: white; border: none; border-radius: 6px; }"
                                "QPushButton:pressed { background: #1565C0; }");

    m_exitBtn = new QPushButton("退出", bottomBar);
    m_exitBtn->setFixedSize(100, 42);
    m_exitBtn->setFont(btnFont);
    m_exitBtn->setStyleSheet("QPushButton { background: #3a3a5e; color: #ccc; border: none; border-radius: 6px; }"
                             "QPushButton:pressed { background: #222; }");
    connect(m_exitBtn, &QPushButton::clicked, qApp, &QApplication::quit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(12, 4, 12, 10);
    btnRow->setSpacing(8);
    btnRow->addStretch();
    btnRow->addWidget(m_btn);
    btnRow->addWidget(m_photoBtn);
    btnRow->addWidget(m_recordBtn);
    btnRow->addWidget(m_galleryBtn);
    btnRow->addWidget(m_exitBtn);
    btnRow->addStretch();

    QVBoxLayout *bottomLayout = new QVBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);
    bottomLayout->addLayout(statusRow);
    bottomLayout->addLayout(sliderRow);
    bottomLayout->addLayout(btnRow);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_label, 1);
    layout->addWidget(bottomBar);

    const char *devices[] = {"/dev/video1", "/dev/video0"};
    const char *device = devices[0];
    struct stat st;
    if (stat(devices[0], &st) != 0) {
        device = devices[1];
    }
    m_capture = new V4l2Capture(device, 640, 480, this);
    m_recorder = new MjpegRecorder(this);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CameraWidget::onCapture);
    connect(m_btn,  &QPushButton::clicked, this, &CameraWidget::onStartStop);
    connect(m_photoBtn, &QPushButton::clicked, this, &CameraWidget::onPhoto);
    connect(m_recordBtn, &QPushButton::clicked, this, &CameraWidget::onRecord);
    connect(m_galleryBtn, &QPushButton::clicked, this, &CameraWidget::onGallery);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &CameraWidget::onBrightnessChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &CameraWidget::onContrastChanged);
    connect(m_saturationSlider, &QSlider::valueChanged, this, &CameraWidget::onSaturationChanged);
}

CameraWidget::~CameraWidget()
{
    m_timer->stop();
    if (m_recorder->isRecording())
        m_recorder->stopRecording();
    m_capture->stop();
}

void CameraWidget::onCapture()
{
    QImage frame = m_capture->grabFrame();
    if (frame.isNull())
        return;

    if (m_pixmap.isNull())
        m_pixmap = QPixmap(m_capture->width(), m_capture->height());
    m_pixmap.convertFromImage(frame);
    m_label->setPixmap(m_pixmap);

    if (m_recorder->isRecording())
    {
        m_recorder->writeFrame(frame.bits(), m_capture->width(), m_capture->height());
    }

    m_frameCount++;
    if (!m_fpsTimer.isValid())
    {
        m_fpsTimer.start();
    } 
    else 
    {
        qint64 elapsed = m_fpsTimer.elapsed();
        if (elapsed >= 1000) 
        {
            double fps = m_frameCount * 1000.0 / elapsed;
            m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
            m_frameCount = 0;
            m_fpsTimer.restart();
        }
    }
}

void CameraWidget::onStartStop()
{
    if (!m_active)
    {
        if (!m_capture->open()) 
        {
            qWarning("Failed to open camera");
            return;
        }
        if (!m_capture->start()) 
        {
            qWarning("Failed to start capture");
            return;
        }
        m_active = true;
        m_btn->setText("停止");
        m_photoBtn->setEnabled(true);
        m_recordBtn->setEnabled(true);
        m_brightnessSlider->setEnabled(true);
        m_contrastSlider->setEnabled(true);
        m_saturationSlider->setEnabled(true);
        m_brightnessMinus->setEnabled(true);
        m_brightnessPlus->setEnabled(true);
        m_contrastMinus->setEnabled(true);
        m_contrastPlus->setEnabled(true);
        m_saturationMinus->setEnabled(true);
        m_saturationPlus->setEnabled(true);
        m_frameCount = 0;
        m_fpsTimer.start();
        m_fpsLabel->setText("FPS: --");
        m_timer->start(33);
    } 
    else 
    {
        if (m_recorder->isRecording())
            m_recorder->stopRecording();
        m_timer->stop();
        m_capture->stop();
        m_capture->close();
        m_active = false;
        m_btn->setText("开始");
        m_photoBtn->setEnabled(false);
        m_recordBtn->setEnabled(false);
        m_brightnessSlider->setEnabled(false);
        m_contrastSlider->setEnabled(false);
        m_saturationSlider->setEnabled(false);
        m_brightnessMinus->setEnabled(false);
        m_brightnessPlus->setEnabled(false);
        m_contrastMinus->setEnabled(false);
        m_contrastPlus->setEnabled(false);
        m_saturationMinus->setEnabled(false);
        m_saturationPlus->setEnabled(false);
        m_recordBtn->setText("录像");
        m_fpsLabel->setText("FPS: --");
        m_recLabel->setText("");
        m_recLabel->hide();
        m_label->setPixmap(QPixmap());
        m_label->setText("Camera Stopped");
    }
}

void CameraWidget::onPhoto()
{
    if (!m_active)
        return;

    QDir dir(kSaveDir);
    if (!dir.exists())
        dir.mkpath(".");

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filePath = QString("%1/photo_%2.jpg").arg(kSaveDir, timestamp);

    QImage frame = m_capture->grabFrame();
    if (frame.isNull()) 
    {
        qWarning("Photo: failed to grab frame");
        return;
    }

    if (frame.save(filePath, "JPEG", 90)) 
    {
        qDebug("Photo saved: %s", qPrintable(filePath));
        m_fpsLabel->setText(QString("Photo saved: photo_%1.jpg").arg(timestamp));
        QTimer::singleShot(2000, this, [this]() {
            m_fpsLabel->setText("FPS: --");
        });
    }
    else 
    {
        qWarning("Photo: failed to save %s", qPrintable(filePath));
    }
}

void CameraWidget::onRecord()
{
    if (!m_active)
        return;

    if (m_recorder->isRecording())
    {
        m_recorder->stopRecording();
        m_recordBtn->setText("录像");
        m_recLabel->setText("");
        m_recLabel->hide();
        m_recTimer.invalidate();
        return;
    }

    QDir dir(kSaveDir);
    if (!dir.exists())
        dir.mkpath(".");

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filePath = QString("%1/video_%2.avi").arg(kSaveDir, timestamp);

    if (m_recorder->startRecording(filePath, m_capture->width(),
                                    m_capture->height(), 30, 85)) 
    {
        m_recordBtn->setText("停止录像");
        m_recLabel->setText("REC 00:00");
        m_recLabel->show();
        m_recTimer.start();
        connect(m_recorder, &MjpegRecorder::durationChanged, this, [this](int seconds) {
            int min = seconds / 60;
            int sec = seconds % 60;
            m_recLabel->setText(QString("REC %1:%2")
                                .arg(min, 2, 10, QChar('0'))
                                .arg(sec, 2, 10, QChar('0')));
        });
        connect(m_recorder, &MjpegRecorder::recordingStopped, this, [this](const QString &path) {
            m_fpsLabel->setText(QString("Video saved: %1").arg(QFileInfo(path).fileName()));
            QTimer::singleShot(3000, this, [this]() {
                m_fpsLabel->setText("FPS: --");
            });
        });
    }
}

void CameraWidget::onGallery()
{
    GalleryDialog dlg(kSaveDir, this);
    dlg.exec();
}

void CameraWidget::onBrightnessChanged(int value)
{
    applyControl(V4L2_CID_BRIGHTNESS, value);
}

void CameraWidget::onContrastChanged(int value)
{
    applyControl(V4L2_CID_CONTRAST, value);
}

void CameraWidget::onSaturationChanged(int value)
{
    applyControl(V4L2_CID_SATURATION, value);
}

void CameraWidget::applyControl(uint32_t id, int value)
{
    if (!m_active)
        return;
    m_capture->setControl(id, value);
}