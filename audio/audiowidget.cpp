#include "audiowidget.h"
#include "spectrumwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QFontDatabase>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QApplication>

AudioWidget::AudioWidget(QWidget *parent)
    : QWidget(parent)
    , m_mixer(nullptr)
    , m_pcmPlayback(nullptr)
    , m_playbackTimer(nullptr)
    , m_spectrum(nullptr)
    , m_speakerMuteCheck(nullptr)
    , m_headphoneMuteCheck(nullptr)
    , m_playing(false)
    , m_paused(false)
    , m_dataStart(0)
    , m_dataSize(0)
    , m_channel(0)
    , m_currentIndex(-1)
    , m_isMp3(false)
    , m_mp3SampleRate(0)
    , m_mp3Channels(0)
    , m_mp3DataOffset(0)
    , m_madInputLen(0)
    , m_madGuard(false)
{
    setWindowTitle("WM8960 Audio");

    initMixer();

    m_playbackTimer = new QTimer(this);
    connect(m_playbackTimer, &QTimer::timeout, this, &AudioWidget::onPlaybackTick);

    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    setupUi();
    updateButtonStates();

    qDebug() << "DEBUG: AudioWidget constructed, UI ready";
}

AudioWidget::~AudioWidget()
{
    qDebug() << "DEBUG: ~AudioWidget() cleaning up...";
    if (m_playing || m_paused)
        onStop();
    if (m_mixer) 
    {
        snd_mixer_close(m_mixer);
        qDebug() << "DEBUG: mixer closed";
    }
}

void AudioWidget::initMixer()
{
    if (snd_mixer_open(&m_mixer, 0) < 0) 
    {
        qWarning() << "ALSA: cannot open mixer";
        m_mixer = nullptr;
        return;
    }
    snd_mixer_attach(m_mixer, "default");
    snd_mixer_selem_register(m_mixer, nullptr, nullptr);
    snd_mixer_load(m_mixer);

    qDebug() << "DEBUG: mixer opened, setting initial levels...";

    mixerSetVolume("Speaker",    127);
    mixerSetSwitch("Speaker",    true);
    mixerSetVolume("Speaker AC", 5);
    mixerSetVolume("Speaker DC", 5);
    mixerSetVolume("Playback",   200);

    mixerSetSwitch("Left Output Mixer PCM",  true);
    mixerSetSwitch("Right Output Mixer PCM", true);
    mixerSetSwitch("Mono Output Mixer Left",  true);
    mixerSetSwitch("Mono Output Mixer Right", true);

    qDebug() << "WM8960 mixer initialized";
}

void AudioWidget::mixerSetVolume(const char *name, long value)
{
    if (!m_mixer) 
    { 
        qWarning() << "mixerSetVolume: mixer is null"; 
        return; 
    }
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, name);
    snd_mixer_elem_t *elem = snd_mixer_find_selem(m_mixer, sid);
    if (elem) 
    {
        if (snd_mixer_selem_has_playback_volume(elem)) 
        {
            snd_mixer_selem_set_playback_volume_all(elem, value);
        } else if (snd_mixer_selem_has_capture_volume(elem)) 
        {
            snd_mixer_selem_set_capture_volume_all(elem, value);
        }
        qDebug() << "  mixerSetVolume:" << name << "=" << value;
    } 
    else 
    {
        qWarning() << "  mixerSetVolume: element" << name << "not found";
    }
}

void AudioWidget::mixerSetSwitch(const char *name, bool on)
{
    if (!m_mixer) 
    { 
        qWarning() << "mixerSetSwitch: mixer is null"; 
        return; 
    }
    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, name);
    snd_mixer_elem_t *elem = snd_mixer_find_selem(m_mixer, sid);
    if (elem) 
    {
        if (snd_mixer_selem_has_playback_switch(elem)) 
        {
            snd_mixer_selem_set_playback_switch_all(elem, on ? 1 : 0);
        } 
        else if (snd_mixer_selem_has_capture_switch(elem)) 
        {
            snd_mixer_selem_set_capture_switch_all(elem, on ? 1 : 0);
        } 
        else 
        {
            qWarning() << "  mixerSetSwitch:" << name << "has NO switch!";
            return;
        }
        qDebug() << "  mixerSetSwitch:" << name << "=" << (on ? "on" : "off");
    } 
    else 
    {
        qWarning() << "  mixerSetSwitch: element" << name << "not found";
    }
}

void AudioWidget::onMixerVolumeChanged(int value)
{
    QSlider *slider = qobject_cast<QSlider *>(sender());
    if (!slider) return;

    if (slider == m_speakerSlider) 
    {
        mixerSetVolume("Speaker", value);
        m_speakerVal->setText(QString::number(value));
        qDebug() << "DEBUG: Speaker volume changed to" << value;
    } 
    else if (slider == m_headphoneSlider) 
    {
        mixerSetVolume("Headphone", value);
        m_headphoneVal->setText(QString::number(value));
        qDebug() << "DEBUG: Headphone volume changed to" << value;
    } 
    else if (slider == m_playbackSlider) 
    {
        mixerSetVolume("Playback", value);
        m_playbackVal->setText(QString::number(value));
        qDebug() << "DEBUG: Playback volume changed to" << value;
    } 
    else if (slider == m_speakerACSlider) 
    {
        mixerSetVolume("Speaker AC", value);
        m_speakerACVal->setText(QString::number(value));
        qDebug() << "DEBUG: Speaker AC changed to" << value;
    } 
    else if (slider == m_speakerDCSlider) 
    {
        mixerSetVolume("Speaker DC", value);
        m_speakerDCVal->setText(QString::number(value));
        qDebug() << "DEBUG: Speaker DC changed to" << value;
    }
}

void AudioWidget::onMixerSwitchToggled(bool checked)
{
    QCheckBox *cb = qobject_cast<QCheckBox *>(sender());
    if (!cb) return;

    if (cb == m_pcmLeftCheck) 
    {
        mixerSetSwitch("Left Output Mixer PCM", checked);
        qDebug() << "DEBUG: PCM Left" << (checked ? "ON" : "OFF");
    } 
    else if (cb == m_pcmRightCheck) 
    {
        mixerSetSwitch("Right Output Mixer PCM", checked);
        qDebug() << "DEBUG: PCM Right" << (checked ? "ON" : "OFF");
    } 
    else if (cb == m_monoLeftCheck) 
    {
        mixerSetSwitch("Mono Output Mixer Left", checked);
        qDebug() << "DEBUG: Mono Left" << (checked ? "ON" : "OFF");
    } 
    else if (cb == m_monoRightCheck) 
    {
        mixerSetSwitch("Mono Output Mixer Right", checked);
        qDebug() << "DEBUG: Mono Right" << (checked ? "ON" : "OFF");
    } 
    else if (cb == m_speakerMuteCheck) 
    {
        mixerSetSwitch("Speaker", checked);
        qDebug() << "DEBUG: Speaker" << (checked ? "ON" : "OFF");
    } 
    else if (cb == m_headphoneMuteCheck) 
    {
        mixerSetSwitch("Headphone", checked);
        qDebug() << "DEBUG: Headphone" << (checked ? "ON" : "OFF");
    }
}

void AudioWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFont titleFont("WenQuanYi Zen Hei", 16, QFont::Bold);
    QFont btnFont("WenQuanYi Zen Hei", 14, QFont::Bold);
    QFont labelFont("WenQuanYi Zen Hei", 12);

    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setFont(titleFont);
    m_statusLabel->setStyleSheet("color: #333; padding: 8px; background: #e8e8e8;");
    mainLayout->addWidget(m_statusLabel);

    QTabWidget *tabs = new QTabWidget(this);
    tabs->setFont(labelFont);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #ccc; }"
        "QTabBar::tab { padding: 8px 24px; font-weight: bold; }"
        "QTabBar::tab:selected { background: #2196F3; color: white; }");

    // ============================================================
    // Tab 1: 播放/录音
    // ============================================================
    QWidget *playTab = new QWidget;
    QVBoxLayout *playLayout = new QVBoxLayout(playTab);
    playLayout->setSpacing(6);

    m_fileLabel = new QLabel("未选择文件", this);
    m_fileLabel->setAlignment(Qt::AlignCenter);
    m_fileLabel->setFont(labelFont);
    m_fileLabel->setStyleSheet("color: #666; padding: 3px;");
    playLayout->addWidget(m_fileLabel);

    m_infoLabel = new QLabel("采样率: --  位深: --  声道: --", this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setFont(labelFont);
    m_infoLabel->setStyleSheet("color: #888; padding: 3px;");
    playLayout->addWidget(m_infoLabel);

    m_spectrum = new SpectrumWidget(this);
    m_spectrum->setMinimumHeight(100);
    m_spectrum->setMaximumHeight(120);
    playLayout->addWidget(m_spectrum);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: #ddd; border: 1px solid #aaa; border-radius: 3px; }"
        "QProgressBar::chunk { background: #4CAF50; border-radius: 3px; }");
    playLayout->addWidget(m_progressBar);

    m_openBtn = new QPushButton("打开文件", this);
    m_openBtn->setFont(btnFont);
    m_openBtn->setFixedHeight(42);
    m_openBtn->setStyleSheet("QPushButton { background: #607D8B; color: white; border-radius: 5px; }"
                             "QPushButton:pressed { background: #455A64; }");
    connect(m_openBtn, &QPushButton::clicked, this, &AudioWidget::onOpenFile);
    playLayout->addWidget(m_openBtn);

    QHBoxLayout *btnRow = new QHBoxLayout;
    m_prevBtn = new QPushButton("上一首", this);
    m_prevBtn->setFont(btnFont);
    m_prevBtn->setFixedHeight(42);
    m_prevBtn->setStyleSheet("QPushButton { background: #9C27B0; color: white; border-radius: 5px; }"
                             "QPushButton:pressed { background: #7B1FA2; }"
                             "QPushButton:disabled { background: #CE93D8; }");
    connect(m_prevBtn, &QPushButton::clicked, this, &AudioWidget::onPrev);
    btnRow->addWidget(m_prevBtn);

    m_playBtn = new QPushButton("播放", this);
    m_playBtn->setFont(btnFont);
    m_playBtn->setFixedHeight(42);
    m_playBtn->setStyleSheet("QPushButton { background: #4CAF50; color: white; border-radius: 5px; }"
                             "QPushButton:pressed { background: #388E3C; }"
                             "QPushButton:disabled { background: #A5D6A7; }");
    connect(m_playBtn, &QPushButton::clicked, this, &AudioWidget::onPlay);
    btnRow->addWidget(m_playBtn);

    m_pauseBtn = new QPushButton("暂停", this);
    m_pauseBtn->setFont(btnFont);
    m_pauseBtn->setFixedHeight(42);
    m_pauseBtn->setStyleSheet("QPushButton { background: #FF9800; color: white; border-radius: 5px; }"
                              "QPushButton:pressed { background: #F57C00; }"
                              "QPushButton:disabled { background: #FFCC80; }");
    connect(m_pauseBtn, &QPushButton::clicked, this, &AudioWidget::onPause);
    btnRow->addWidget(m_pauseBtn);

    m_stopBtn = new QPushButton("停止", this);
    m_stopBtn->setFont(btnFont);
    m_stopBtn->setFixedHeight(42);
    m_stopBtn->setStyleSheet("QPushButton { background: #F44336; color: white; border-radius: 5px; }"
                             "QPushButton:pressed { background: #D32F2F; }"
                             "QPushButton:disabled { background: #EF9A9A; }");
    connect(m_stopBtn, &QPushButton::clicked, this, &AudioWidget::onStop);
    btnRow->addWidget(m_stopBtn);

    m_nextBtn = new QPushButton("下一首", this);
    m_nextBtn->setFont(btnFont);
    m_nextBtn->setFixedHeight(42);
    m_nextBtn->setStyleSheet("QPushButton { background: #9C27B0; color: white; border-radius: 5px; }"
                             "QPushButton:pressed { background: #7B1FA2; }"
                             "QPushButton:disabled { background: #CE93D8; }");
    connect(m_nextBtn, &QPushButton::clicked, this, &AudioWidget::onNext);
    btnRow->addWidget(m_nextBtn);
    playLayout->addLayout(btnRow);

    QHBoxLayout *volRow = new QHBoxLayout;
    QLabel *volLabel = new QLabel("音量", this);
    volLabel->setFont(labelFont);
    volLabel->setFixedWidth(50);
    volRow->addWidget(volLabel);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: #ddd; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 20px; height: 20px; margin: -7px 0; "
        "background: #2196F3; border-radius: 10px; }"
        "QSlider::sub-page:horizontal { background: #2196F3; border-radius: 3px; }");
    connect(m_volumeSlider, &QSlider::valueChanged, this, &AudioWidget::onVolumeChanged);
    volRow->addWidget(m_volumeSlider);

    QLabel *volVal = new QLabel("80%", this);
    volVal->setFont(labelFont);
    volVal->setFixedWidth(50);
    connect(m_volumeSlider, &QSlider::valueChanged, volVal,
            [volVal](int v) { volVal->setText(QString("%1%").arg(v)); });
    volRow->addWidget(volVal);

    QLabel *chLabel = new QLabel("声道", this);
    chLabel->setFont(labelFont);
    chLabel->setFixedWidth(50);
    volRow->addWidget(chLabel);

    m_channelCombo = new QComboBox(this);
    m_channelCombo->setFont(labelFont);
    m_channelCombo->addItem("立体声");
    m_channelCombo->addItem("左声道");
    m_channelCombo->addItem("右声道");
    m_channelCombo->setStyleSheet(
        "QComboBox { padding: 3px; border: 1px solid #aaa; border-radius: 3px; }"
        "QComboBox::drop-down { border: none; }");
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioWidget::onChannelChanged);
    volRow->addWidget(m_channelCombo);
    playLayout->addLayout(volRow);

    playLayout->addStretch();
    tabs->addTab(playTab, "播放");

    // ============================================================
    // Tab 2: 混音器设置
    // ============================================================
    QWidget *settingsTab = new QWidget;
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsTab);
    settingsLayout->setSpacing(6);

    auto makeSliderRow = [&](const QString &label, QSlider *&slider, QLabel *&val,
                              int min, int max, int def) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *lbl = new QLabel(label, this);
        lbl->setFont(labelFont);
        lbl->setFixedWidth(80);
        row->addWidget(lbl);
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(min, max);
        slider->setValue(def);
        slider->setStyleSheet(
            "QSlider::groove:horizontal { height: 4px; background: #ddd; border-radius: 2px; }"
            "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; "
            "background: #FF5722; border-radius: 8px; }"
            "QSlider::sub-page:horizontal { background: #FF5722; border-radius: 2px; }");
        connect(slider, &QSlider::valueChanged, this, &AudioWidget::onMixerVolumeChanged);
        row->addWidget(slider);
        val = new QLabel(QString::number(def), this);
        val->setFont(labelFont);
        val->setFixedWidth(40);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(val);
        settingsLayout->addLayout(row);
    };

    QLabel *outLabel = new QLabel("— 输出设置 —", this);
    outLabel->setFont(labelFont);
    outLabel->setStyleSheet("color: #555; padding: 3px 0;");
    settingsLayout->addWidget(outLabel);

    makeSliderRow("扬声器",  m_speakerSlider,  m_speakerVal,  0, 127, 127);
    makeSliderRow("耳机",    m_headphoneSlider, m_headphoneVal, 0, 127, 127);
    makeSliderRow("播放增益", m_playbackSlider, m_playbackVal, 0, 255, 200);
    makeSliderRow("AC增益",  m_speakerACSlider, m_speakerACVal, 0, 5,   5);
    makeSliderRow("DC增益",  m_speakerDCSlider, m_speakerDCVal, 0, 5,   5);

    QHBoxLayout *outSwRow = new QHBoxLayout;
    m_speakerMuteCheck = new QCheckBox("扬声器开关", this);
    m_speakerMuteCheck->setFont(labelFont);
    m_speakerMuteCheck->setChecked(true);
    connect(m_speakerMuteCheck, &QCheckBox::toggled, this, &AudioWidget::onMixerSwitchToggled);
    outSwRow->addWidget(m_speakerMuteCheck);

    m_headphoneMuteCheck = new QCheckBox("耳机开关", this);
    m_headphoneMuteCheck->setFont(labelFont);
    m_headphoneMuteCheck->setChecked(true);
    connect(m_headphoneMuteCheck, &QCheckBox::toggled, this, &AudioWidget::onMixerSwitchToggled);
    outSwRow->addWidget(m_headphoneMuteCheck);
    settingsLayout->addLayout(outSwRow);

    QHBoxLayout *pcmSwRow = new QHBoxLayout;
    m_pcmLeftCheck = new QCheckBox("PCM左", this);
    m_pcmLeftCheck->setFont(labelFont);
    m_pcmLeftCheck->setChecked(true);
    connect(m_pcmLeftCheck, &QCheckBox::toggled, this, &AudioWidget::onMixerSwitchToggled);
    pcmSwRow->addWidget(m_pcmLeftCheck);

    m_pcmRightCheck = new QCheckBox("PCM右", this);
    m_pcmRightCheck->setFont(labelFont);
    m_pcmRightCheck->setChecked(true);
    connect(m_pcmRightCheck, &QCheckBox::toggled, this, &AudioWidget::onMixerSwitchToggled);
    pcmSwRow->addWidget(m_pcmRightCheck);

    m_monoLeftCheck = new QCheckBox("单声道左", this);
    m_monoLeftCheck->setFont(labelFont);
    m_monoLeftCheck->setChecked(true);
    connect(m_monoLeftCheck, &QCheckBox::toggled, this, &AudioWidget::onMixerSwitchToggled);
    pcmSwRow->addWidget(m_monoLeftCheck);

    m_monoRightCheck = new QCheckBox("单声道右", this);
    m_monoRightCheck->setFont(labelFont);
    m_monoRightCheck->setChecked(true);
    connect(m_monoRightCheck, &QCheckBox::toggled, this, &AudioWidget::onMixerSwitchToggled);
    pcmSwRow->addWidget(m_monoRightCheck);
    settingsLayout->addLayout(pcmSwRow);

    settingsLayout->addStretch();

    tabs->addTab(settingsTab, "混音器设置");

    mainLayout->addWidget(tabs);

    QPushButton *exitBtn2 = new QPushButton("退出", this);
    exitBtn2->setFont(btnFont);
    exitBtn2->setFixedHeight(42);
    exitBtn2->setStyleSheet("QPushButton { background: white; color: black; border: 1px solid #999; border-radius: 5px; }"
                            "QPushButton:pressed { background: #ddd; }");
    connect(exitBtn2, &QPushButton::clicked, qApp, &QApplication::quit);
    mainLayout->addWidget(exitBtn2);
}

void AudioWidget::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, "选择音频文件",
                                                 "/root", "Audio Files (*.wav *.mp3)");
    if (path.isEmpty())
        return;

    if (m_playing || m_paused)
        onStop();

    scanDirectory(path);

    if (m_playlist.isEmpty()) {
        m_statusLabel->setText("未找到音频文件");
        m_statusLabel->setStyleSheet("color: red; padding: 8px;");
        return;
    }

    m_currentIndex = m_playlist.indexOf(path);
    if (m_currentIndex < 0)
        m_currentIndex = 0;

    if (!loadFile(m_playlist[m_currentIndex])) {
        m_statusLabel->setText("加载失败");
        m_statusLabel->setStyleSheet("color: red; padding: 8px;");
    }
}

void AudioWidget::scanDirectory(const QString &filePath)
{
    m_playlist.clear();
    QDir dir = QFileInfo(filePath).absoluteDir();
    QStringList filters;
    filters << "*.wav" << "*.mp3";
    dir.setNameFilters(filters);
    dir.setSorting(QDir::Name);

    QFileInfoList files = dir.entryInfoList(QDir::Files);
    for (const QFileInfo &fi : files) {
        m_playlist.append(fi.absoluteFilePath());
    }
    qDebug() << "DEBUG: playlist scanned, found" << m_playlist.size() << "files";
}

bool AudioWidget::loadFile(const QString &path)
{
    m_playbackFile.setFileName(path);
    qDebug() << "DEBUG: opening file:" << path;
    if (!m_playbackFile.open(QIODevice::ReadOnly)) {
        qWarning() << "ERROR: cannot open file:" << path;
        return false;
    }

    QString suffix = path.right(4).toLower();
    if (suffix == ".mp3") {
        m_isMp3 = true;
        if (!parseMp3Info()) {
            m_playbackFile.close();
            m_isMp3 = false;
            return false;
        }
    } else {
        m_isMp3 = false;
        if (!parseWavHeader()) {
            m_playbackFile.close();
            return false;
        }
    }

    QFileInfo fi(path);
    m_fileLabel->setText(fi.fileName());
    if (m_isMp3) {
        m_infoLabel->setText(QString("采样率: %1 Hz  声道: %2  (MP3)")
                                 .arg(m_mp3SampleRate)
                                 .arg(m_mp3Channels));
    } else {
        m_infoLabel->setText(QString("采样率: %1 Hz  位深: %2 bit  声道: %3")
                                 .arg(m_header.sampleRate)
                                 .arg(m_header.bitsPerSample)
                                 .arg(m_header.numChannels));
    }
    m_statusLabel->setText("已加载文件");
    m_statusLabel->setStyleSheet("color: #333; padding: 8px;");
    m_progressBar->setValue(0);

    updateButtonStates();
    return true;
}

void AudioWidget::onPrev()
{
    if (m_playlist.isEmpty())
        return;

    bool wasPlaying = m_playing || m_paused;
    if (m_playing || m_paused)
        onStop();

    if (m_currentIndex <= 0)
        m_currentIndex = m_playlist.size() - 1;
    else
        m_currentIndex--;

    if (!loadFile(m_playlist[m_currentIndex]))
        return;

    if (wasPlaying)
        onPlay();
}

void AudioWidget::onNext()
{
    if (m_playlist.isEmpty())
        return;

    bool wasPlaying = m_playing || m_paused;
    if (m_playing || m_paused)
        onStop();

    if (m_currentIndex >= m_playlist.size() - 1)
        m_currentIndex = 0;
    else
        m_currentIndex++;

    if (!loadFile(m_playlist[m_currentIndex]))
        return;

    if (wasPlaying)
        onPlay();
}

bool AudioWidget::parseWavHeader()
{
    m_playbackFile.seek(0);

    char riffHeader[12];
    if (m_playbackFile.read(riffHeader, 12) != 12)
        return false;
    if (qstrncmp(riffHeader, "RIFF", 4) != 0 || qstrncmp(riffHeader + 8, "WAVE", 4) != 0)
        return false;

    char chunkId[4];
    quint32 chunkSize;
    bool foundFmt = false, foundData = false;

    while (m_playbackFile.read(chunkId, 4) == 4 && m_playbackFile.read((char *)&chunkSize, 4) == 4) {
        qDebug() << "DEBUG: WAV chunk:" << QByteArray(chunkId, 4) << "size=" << chunkSize;

        if (qstrncmp(chunkId, "fmt ", 4) == 0) {
            quint16 audioFormat, numChannels, bitsPerSample;
            quint32 sampleRate;
            m_playbackFile.read((char *)&audioFormat, 2);
            m_playbackFile.read((char *)&numChannels, 2);
            m_playbackFile.read((char *)&sampleRate, 4);
            m_playbackFile.seek(m_playbackFile.pos() + 6);
            m_playbackFile.read((char *)&bitsPerSample, 2);
            m_playbackFile.seek(m_playbackFile.pos() + chunkSize - 16);

            m_header.audioFormat  = audioFormat;
            m_header.numChannels  = numChannels;
            m_header.sampleRate   = sampleRate;
            m_header.bitsPerSample = bitsPerSample;

            qDebug() << "DEBUG: WAV fmt: audioFormat=" << audioFormat << "channels=" << numChannels
                     << "rate=" << sampleRate << "bits=" << bitsPerSample;

            if (audioFormat != 1) {
                qWarning() << "ERROR: WAV not PCM, audioFormat=" << audioFormat;
                return false;
            }
            foundFmt = true;
        } else if (qstrncmp(chunkId, "data", 4) == 0) {
            m_dataStart = m_playbackFile.pos();
            m_dataSize  = chunkSize;
            qDebug() << "DEBUG: WAV data: start=" << m_dataStart << "size=" << m_dataSize;
            foundData = true;
            break;
        } else {
            m_playbackFile.seek(m_playbackFile.pos() + chunkSize);
        }
    }

    if (!foundFmt || !foundData) {
        qWarning() << "ERROR: WAV missing fmt or data chunk";
        return false;
    }

    return true;
}

bool AudioWidget::parseMp3Info()
{
    m_playbackFile.seek(0);
    qint64 fileSize = m_playbackFile.size();

    unsigned char buf[16384];
    qint64 toRead = fileSize < (qint64)sizeof(buf) ? fileSize : sizeof(buf);
    qint64 bytesRead = m_playbackFile.read((char *)buf, toRead);
    if (bytesRead < 4) {
        return false;
    }

    int offset = 0;
    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
        if (bytesRead < 10) return false;
        unsigned int id3Size = ((buf[6] & 0x7F) << 21)
                             | ((buf[7] & 0x7F) << 14)
                             | ((buf[8] & 0x7F) << 7)
                             |  (buf[9] & 0x7F);
        offset = 10 + id3Size;
        if (offset >= bytesRead) {
            qDebug() << "DEBUG: ID3 tag too large, offset=" << offset;
            m_mp3DataOffset = offset;
            return false;
        }
        qDebug() << "DEBUG: ID3v2 tag found, size=" << id3Size << "data offset=" << offset;
    }

    mad_stream_init(&m_madStream);
    mad_frame_init(&m_madFrame);
    mad_synth_init(&m_madSynth);

    mad_stream_buffer(&m_madStream, buf + offset, bytesRead - offset);
    m_madStream.error = MAD_ERROR_NONE;

    if (mad_frame_decode(&m_madFrame, &m_madStream) < 0) {
        qWarning() << "MP3 first frame decode error:" << mad_stream_errorstr(&m_madStream);
        if (m_madStream.error == MAD_ERROR_BUFLEN) {
            mad_frame_finish(&m_madFrame);
            mad_synth_finish(&m_madSynth);
            mad_stream_finish(&m_madStream);
            return false;
        }
        if (!MAD_RECOVERABLE(m_madStream.error)) {
            mad_frame_finish(&m_madFrame);
            mad_synth_finish(&m_madSynth);
            mad_stream_finish(&m_madStream);
            return false;
        }
        if (mad_frame_decode(&m_madFrame, &m_madStream) < 0) {
            mad_frame_finish(&m_madFrame);
            mad_synth_finish(&m_madSynth);
            mad_stream_finish(&m_madStream);
            return false;
        }
    }

    m_mp3SampleRate = m_madFrame.header.samplerate;
    m_mp3Channels   = MAD_NCHANNELS(&m_madFrame.header);
    m_mp3DataOffset = offset;
    m_dataSize = fileSize;

    qDebug() << "DEBUG: MP3 info: rate=" << m_mp3SampleRate
             << "channels=" << m_mp3Channels
             << "fileSize=" << fileSize
             << "dataOffset=" << m_mp3DataOffset;

    mad_frame_finish(&m_madFrame);
    mad_synth_finish(&m_madSynth);
    mad_stream_finish(&m_madStream);

    return true;
}

inline short AudioWidget::madFixedToS16(mad_fixed_t v)
{
    v += (1L << (MAD_F_FRACBITS - 16));
    if (v >= MAD_F_ONE)
        v = MAD_F_ONE - 1;
    else if (v < -MAD_F_ONE)
        v = -MAD_F_ONE;
    return (short)(v >> (MAD_F_FRACBITS + 1 - 16));
}

void AudioWidget::onPlay()
{
    qDebug() << "DEBUG: onPlay() called, paused=" << m_paused << "playing=" << m_playing;
    if (m_playing) {
        qDebug() << "DEBUG: onPlay() ignored, already playing";
        return;
    }
    if (m_paused) {
        if (m_pcmPlayback) {
            snd_pcm_pause(m_pcmPlayback, 0);
            m_paused = false;
            m_playing = true;
            m_playbackTimer->start(30);
            m_statusLabel->setText("正在播放");
            m_statusLabel->setStyleSheet("color: #4CAF50; padding: 8px;");
        }
        updateButtonStates();
        return;
    }

    if (!m_playbackFile.isOpen())
        return;

    int channels, sampleRate;
    snd_pcm_format_t pcmFmt;
    if (m_isMp3) {
        channels = m_mp3Channels;
        sampleRate = m_mp3SampleRate;
        pcmFmt = SND_PCM_FORMAT_S16_LE;
    } else {
        channels = m_header.numChannels;
        sampleRate = m_header.sampleRate;
        pcmFmt = (m_header.bitsPerSample == 16)
            ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8;
    }

    int err = snd_pcm_open(&m_pcmPlayback, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        qWarning() << "ALSA playback open error:" << snd_strerror(err);
        m_pcmPlayback = nullptr;
        return;
    }

    snd_pcm_hw_params_t *hwParams;
    snd_pcm_hw_params_alloca(&hwParams);
    snd_pcm_hw_params_any(m_pcmPlayback, hwParams);
    snd_pcm_hw_params_set_access(m_pcmPlayback, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(m_pcmPlayback, hwParams, pcmFmt);
    snd_pcm_hw_params_set_channels(m_pcmPlayback, hwParams, channels);

    unsigned int rate = sampleRate;
    snd_pcm_hw_params_set_rate_near(m_pcmPlayback, hwParams, &rate, 0);

    snd_pcm_uframes_t periodSize = 1024;
    snd_pcm_hw_params_set_period_size_near(m_pcmPlayback, hwParams, &periodSize, 0);

    snd_pcm_uframes_t bufferSize = 4096;
    snd_pcm_hw_params_set_buffer_size_near(m_pcmPlayback, hwParams, &bufferSize);

    err = snd_pcm_hw_params(m_pcmPlayback, hwParams);
    if (err < 0) {
        qWarning() << "ALSA hw_params error:" << snd_strerror(err);
        snd_pcm_close(m_pcmPlayback);
        m_pcmPlayback = nullptr;
        return;
    }

    qDebug() << "ALSA playback opened: rate=" << rate
             << "channels=" << channels
             << "periodSize=" << periodSize
             << "bufferSize=" << bufferSize;

    qDebug() << "DEBUG: re-applying speaker mixer after PCM open";
    mixerSetVolume("Speaker",    127);
    mixerSetVolume("Speaker AC", 5);
    mixerSetVolume("Speaker DC", 5);
    mixerSetVolume("Playback",   200);
    mixerSetSwitch("Left Output Mixer PCM",  true);
    mixerSetSwitch("Right Output Mixer PCM", true);
    mixerSetSwitch("Mono Output Mixer Left",  true);
    mixerSetSwitch("Mono Output Mixer Right", true);

    snd_pcm_state_t state = snd_pcm_state(m_pcmPlayback);
    qDebug() << "DEBUG: PCM state after hw_params:" << snd_pcm_state_name(state);

    snd_pcm_prepare(m_pcmPlayback);
    int totalFrameSize = channels * ((m_isMp3) ? 2 : (m_header.bitsPerSample / 8));
    QByteArray silence(bufferSize * totalFrameSize, 0);
    snd_pcm_sframes_t primed = snd_pcm_writei(m_pcmPlayback, silence.data(), bufferSize);
    if (primed < 0) {
        qWarning() << "DEBUG: PCM prime write failed:" << snd_strerror(primed);
        primed = snd_pcm_recover(m_pcmPlayback, primed, 0);
        if (primed < 0) {
            qWarning() << "DEBUG: PCM recover failed:" << snd_strerror(primed);
            snd_pcm_close(m_pcmPlayback);
            m_pcmPlayback = nullptr;
            return;
        }
        primed = snd_pcm_writei(m_pcmPlayback, silence.data(), periodSize);
        if (primed < 0) {
            qWarning() << "DEBUG: PCM retry write failed:" << snd_strerror(primed);
            snd_pcm_close(m_pcmPlayback);
            m_pcmPlayback = nullptr;
            return;
        }
    }
    state = snd_pcm_state(m_pcmPlayback);
    qDebug() << "DEBUG: PCM primed with" << primed << "frames, state:" << snd_pcm_state_name(state);

    if (m_isMp3) {
        m_playbackFile.seek(m_mp3DataOffset);
        mad_stream_init(&m_madStream);
        mad_frame_init(&m_madFrame);
        mad_synth_init(&m_madSynth);
        m_madPcmBuf.clear();
        m_madGuard = true;
    } else {
        m_playbackFile.seek(m_dataStart);
    }

    m_playing = true;
    m_paused = false;
    m_spectrum->setSampleRate(sampleRate);
    m_spectrum->start();
    m_playbackTimer->start(30);
    m_statusLabel->setText("正在播放");
    m_statusLabel->setStyleSheet("color: #4CAF50; padding: 8px;");

    updateButtonStates();
}

void AudioWidget::onPause()
{
    qDebug() << "DEBUG: onPause() called, playing=" << m_playing;
    if (m_pcmPlayback && m_playing) {
        snd_pcm_pause(m_pcmPlayback, 1);
            m_playing = false;
            m_paused = true;
            m_playbackTimer->stop();
            m_spectrum->stop();
        m_statusLabel->setText("已暂停");
        m_statusLabel->setStyleSheet("color: #FF9800; padding: 8px;");
    }
    updateButtonStates();
}

void AudioWidget::onStop()
{
    qDebug() << "DEBUG: onStop() called";
    m_playbackTimer->stop();
    m_spectrum->stop();
    if (m_pcmPlayback) {
        snd_pcm_drop(m_pcmPlayback);
        snd_pcm_close(m_pcmPlayback);
        m_pcmPlayback = nullptr;
    }
    if (m_isMp3 && m_madGuard) {
        mad_frame_finish(&m_madFrame);
        mad_synth_finish(&m_madSynth);
        mad_stream_finish(&m_madStream);
        m_madGuard = false;
    }
    m_playing = false;
    m_paused = false;
    m_progressBar->setValue(0);
    m_statusLabel->setText("已停止");
    m_statusLabel->setStyleSheet("color: #333; padding: 8px;");

    updateButtonStates();
}

void AudioWidget::onVolumeChanged(int volume)
{
    Q_UNUSED(volume);
}

void AudioWidget::onChannelChanged(int index)
{
    m_channel = index;
    qDebug() << "Channel changed to:" << channelName(index);
}

QString AudioWidget::channelName(int index)
{
    switch (index) {
    case 0: return "立体声";
    case 1: return "左声道";
    case 2: return "右声道";
    default: return "未知";
    }
}

void AudioWidget::onPlaybackTick()
{
    if (!m_playing || !m_pcmPlayback)
        return;

    static int tickCount = 0;
    tickCount++;

    if (m_isMp3) {
        onPlaybackTickMp3(tickCount);
    } else {
        onPlaybackTickWav(tickCount);
    }
}

void AudioWidget::onPlaybackTickWav(int tickCount)
{
    int frameSize = (m_header.bitsPerSample / 8) * m_header.numChannels;
    int bufFrames = 2048;
    int bufBytes = bufFrames * frameSize;
    QByteArray buf(bufBytes, 0);

    qint64 bytesRead = m_playbackFile.read(buf.data(), bufBytes);
    if (bytesRead <= 0) {
        qDebug() << "DEBUG: playback finished, total ticks=" << tickCount;
        onStop();
        QTimer::singleShot(100, this, &AudioWidget::onNext);
        return;
    }

    int frames = bytesRead / frameSize;
    m_spectrum->feedPcm((const short *)buf.constData(), frames * m_header.numChannels);
    snd_pcm_sframes_t written = snd_pcm_writei(m_pcmPlayback, buf.data(), frames);
    if (written < 0) {
        written = snd_pcm_recover(m_pcmPlayback, written, 0);
        if (written < 0) {
            qWarning() << "DEBUG: ALSA write error:" << snd_strerror(written) << "tick=" << tickCount;
            onStop();
            return;
        }
        qWarning() << "DEBUG: ALSA underrun recovered, tick=" << tickCount;
        m_playbackFile.seek(m_playbackFile.pos() - bytesRead);
        return;
    }

    if (tickCount % 100 == 0)
        qDebug() << "DEBUG: playback tick" << tickCount << "written=" << written << "frames";

    qint64 pos = m_playbackFile.pos() - m_dataStart;
    if (pos >= 0 && m_dataSize > 0)
        m_progressBar->setValue(static_cast<int>(pos * 1000 / m_dataSize));
}

void AudioWidget::onPlaybackTickMp3(int tickCount)
{
    int channels = m_mp3Channels;
    int frameSize = channels * 2;

    m_madPcmBuf.clear();

    for (int frame = 0; frame < 4; frame++) {
        if (m_madInputLen > 0 && m_madStream.next_frame) {
            size_t unconsumed = m_madStream.bufend - m_madStream.next_frame;
            if (unconsumed > 0 && unconsumed < sizeof(m_madInputBuf)) {
                memmove(m_madInputBuf, m_madStream.next_frame, unconsumed);
                m_madInputLen = (int)unconsumed;
            } else {
                m_madInputLen = 0;
            }
        } else {
            m_madInputLen = 0;
        }

        qint64 remaining = m_playbackFile.size() - m_playbackFile.pos();
        int space = sizeof(m_madInputBuf) - m_madInputLen;
        int toRead = remaining < (qint64)space ? (int)remaining : space;

        if (toRead > 0) {
            qint64 bytesRead = m_playbackFile.read((char *)(m_madInputBuf + m_madInputLen), toRead);
            if (bytesRead > 0)
                m_madInputLen += (int)bytesRead;
        }

        if (m_madInputLen <= 0) {
            break;
        }

        mad_stream_buffer(&m_madStream, m_madInputBuf, m_madInputLen);
        m_madStream.error = MAD_ERROR_NONE;

        if (mad_frame_decode(&m_madFrame, &m_madStream) < 0) {
            if (m_madStream.error == MAD_ERROR_BUFLEN)
                break;
            if (!MAD_RECOVERABLE(m_madStream.error)) {
                if (tickCount <= 3)
                    qWarning() << "MP3 unrecoverable error:" << mad_stream_errorstr(&m_madStream);
                onStop();
                return;
            }
            continue;
        }

        mad_synth_frame(&m_madSynth, &m_madFrame);

        int synthFrames = m_madSynth.pcm.length;
        int oldSize = m_madPcmBuf.size();
        m_madPcmBuf.resize(oldSize + synthFrames * frameSize);
        short *dst = (short *)(m_madPcmBuf.data() + oldSize);
        for (int i = 0; i < synthFrames; i++) {
            for (int ch = 0; ch < channels; ch++) {
                *dst++ = madFixedToS16(m_madSynth.pcm.samples[ch][i]);
            }
        }
    }

    int totalFrames = m_madPcmBuf.size() / frameSize;
    if (totalFrames <= 0) {
        if (m_playbackFile.pos() >= m_playbackFile.size()) {
            qDebug() << "DEBUG: MP3 playback finished, ticks=" << tickCount;
            onStop();
            QTimer::singleShot(100, this, &AudioWidget::onNext);
        }
        return;
    }

    const short *src = (const short *)m_madPcmBuf.constData();
    m_spectrum->feedPcm(src, totalFrames * channels);
    snd_pcm_sframes_t written = snd_pcm_writei(m_pcmPlayback, src, totalFrames);
    if (written < 0) {
        if (tickCount <= 3)
            qWarning() << "DEBUG: ALSA write error (mp3) tick=" << tickCount << ":" << snd_strerror(written);
        written = snd_pcm_recover(m_pcmPlayback, written, 0);
        if (written < 0) {
            qWarning() << "DEBUG: ALSA recover failed (mp3):" << snd_strerror(written);
            onStop();
            return;
        }
        if (tickCount <= 3)
            qDebug() << "DEBUG: ALSA recovered (mp3), retrying write...";
        written = snd_pcm_writei(m_pcmPlayback, src, totalFrames);
        if (written < 0) {
            qWarning() << "DEBUG: ALSA retry write also failed:" << snd_strerror(written);
            onStop();
            return;
        }
    }

    if (tickCount <= 3 || tickCount % 100 == 0)
        qDebug() << "DEBUG: MP3 tick" << tickCount << "decoded=" << totalFrames << "written=" << written << "frames";

    qint64 pos = m_playbackFile.pos();
    if (pos >= 0 && m_dataSize > 0)
        m_progressBar->setValue(static_cast<int>(pos * 1000 / m_dataSize));
}

void AudioWidget::updateButtonStates()
{
    bool hasFile = m_playbackFile.isOpen();
    m_playBtn->setEnabled(hasFile);
    m_pauseBtn->setEnabled(m_playing);
    m_stopBtn->setEnabled(m_playing || m_paused);
    m_openBtn->setEnabled(true);
    m_prevBtn->setEnabled(m_playlist.size() > 1);
    m_nextBtn->setEnabled(m_playlist.size() > 1);
}