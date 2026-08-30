#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QObject>
#include <QImage>
#include <linux/videodev2.h>

struct v4l2_buffer_info {
    void  *start;
    size_t length;
};

class V4l2Capture : public QObject
{
    Q_OBJECT
public:
    explicit V4l2Capture(const QString &device = "/dev/video0",
                         int width = 640, int height = 480,
                         QObject *parent = nullptr);
    ~V4l2Capture();

    bool open();
    void close();
    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    QImage grabFrame();

    int  width()  const { return m_width; }
    int  height() const { return m_height; }

    bool queryControl(uint32_t ctrl_id, int *min, int *max, int *step, int *def);
    bool setControl(uint32_t ctrl_id, int value);
    int  getControl(uint32_t ctrl_id);

signals:
    void frameReady(const QImage &frame);

private:
    bool initDevice();
    bool initMmap();
    bool setFormat();
    void yuyvToRgb32(const unsigned char *yuyv, unsigned char *rgb,
                     int width, int height);

    QString  m_device;
    int      m_width;
    int      m_height;
    int      m_fd;
    bool     m_running;

    v4l2_buffer_info *m_buffers;
    unsigned int      m_nbufs;
    QImage            m_frame;
};

#endif