#ifndef MJPEGRECORDER_H
#define MJPEGRECORDER_H

#include <QObject>
#include <QFile>
#include <QElapsedTimer>
#include <QVector>
#include <cstdint>

struct AviIndexEntry {
    uint32_t offset;
    uint32_t size;
};

class MjpegRecorder : public QObject
{
    Q_OBJECT
public:
    explicit MjpegRecorder(QObject *parent = nullptr);
    ~MjpegRecorder();

    bool startRecording(const QString &filePath,
                        int width, int height, int fps, int quality = 85);
    void stopRecording();
    bool isRecording() const { return m_recording; }

    void writeFrame(const unsigned char *rgbData, int width, int height);

signals:
    void recordingStarted();
    void recordingStopped(const QString &filePath);
    void durationChanged(int seconds);

private:
    void writeAviHeader();
    void finalizeAvi();
    bool encodeJpeg(const unsigned char *rgbData, int width, int height,
                    int quality, QByteArray &outJpeg);

    QFile          m_file;
    bool           m_recording;
    int            m_width;
    int            m_height;
    int            m_fps;
    int            m_quality;
    int            m_frameCount;
    uint32_t       m_moviSize;
    int64_t        m_headerPos_moviSize;
    int64_t        m_headerPos_totalSize;
    QVector<AviIndexEntry> m_index;
    QElapsedTimer  m_elapsed;
    QByteArray     m_jpegBuf;
};

#endif