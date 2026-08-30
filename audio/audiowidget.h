#ifndef AUDIOWIDGET_H
#define AUDIOWIDGET_H

#include <QWidget>
#include <QFile>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QCheckBox>
#include <QTabWidget>
#include <QTimer>
#include <alsa/asoundlib.h>
#include <mad.h>

class SpectrumWidget;

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4];
    quint32  fileSize;
    char     wave[4];
    char     fmt[4];
    quint32  fmtSize;
    quint16  audioFormat;
    quint16  numChannels;
    quint32  sampleRate;
    quint32  byteRate;
    quint16  blockAlign;
    quint16  bitsPerSample;
    char     data[4];
    quint32  dataSize;
};
#pragma pack(pop)

class AudioWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AudioWidget(QWidget *parent = nullptr);
    ~AudioWidget();

private slots:
    void onOpenFile();
    void onPlay();
    void onPause();
    void onStop();
    void onPrev();
    void onNext();
    void onVolumeChanged(int volume);
    void onChannelChanged(int index);
    void onPlaybackTick();

    void onMixerVolumeChanged(int value);
    void onMixerSwitchToggled(bool checked);

private:
    void setupUi();
    void initMixer();
    void mixerSetVolume(const char *name, long value);
    void mixerSetSwitch(const char *name, bool on);
    bool loadFile(const QString &path);
    void scanDirectory(const QString &filePath);
    bool parseWavHeader();
    void updateButtonStates();
    QString channelName(int index);
    bool parseMp3Info();
    static inline short madFixedToS16(mad_fixed_t v);
    void onPlaybackTickWav(int tickCount);
    void onPlaybackTickMp3(int tickCount);

    snd_mixer_t   *m_mixer;

    snd_pcm_t     *m_pcmPlayback;
    QTimer        *m_playbackTimer;
    QFile          m_playbackFile;

    QLabel       *m_statusLabel;
    QLabel       *m_fileLabel;
    QLabel       *m_infoLabel;
    SpectrumWidget *m_spectrum;
    QSlider      *m_volumeSlider;
    QComboBox    *m_channelCombo;
    QPushButton  *m_playBtn;
    QPushButton  *m_pauseBtn;
    QPushButton  *m_stopBtn;
    QPushButton  *m_openBtn;
    QPushButton  *m_prevBtn;
    QPushButton  *m_nextBtn;
    QProgressBar *m_progressBar;

    QSlider      *m_speakerSlider;
    QLabel       *m_speakerVal;
    QSlider      *m_headphoneSlider;
    QLabel       *m_headphoneVal;
    QSlider      *m_playbackSlider;
    QLabel       *m_playbackVal;
    QSlider      *m_speakerACSlider;
    QLabel       *m_speakerACVal;
    QSlider      *m_speakerDCSlider;
    QLabel       *m_speakerDCVal;
    QCheckBox    *m_pcmLeftCheck;
    QCheckBox    *m_pcmRightCheck;
    QCheckBox    *m_monoLeftCheck;
    QCheckBox    *m_monoRightCheck;
    QCheckBox    *m_speakerMuteCheck;
    QCheckBox    *m_headphoneMuteCheck;

    WavHeader      m_header;
    bool           m_playing;
    bool           m_paused;
    qint64         m_dataStart;
    qint64         m_dataSize;
    int            m_channel;

    QStringList    m_playlist;
    int            m_currentIndex;

    bool           m_isMp3;
    int            m_mp3SampleRate;
    int            m_mp3Channels;
    qint64         m_mp3DataOffset;
    struct mad_stream  m_madStream;
    struct mad_frame   m_madFrame;
    struct mad_synth   m_madSynth;
    unsigned char      m_madInputBuf[8192];
    int                m_madInputLen;
    QByteArray         m_madPcmBuf;
    bool               m_madGuard;
};

#endif