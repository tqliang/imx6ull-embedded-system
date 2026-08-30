#include "v4l2capture.h"
#include <QDebug>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

V4l2Capture::V4l2Capture(const QString &device, int width, int height,
                         QObject *parent)
    : QObject(parent)
    , m_device(device)
    , m_width(width)
    , m_height(height)
    , m_fd(-1)
    , m_running(false)
    , m_buffers(nullptr)
    , m_nbufs(0)
{
}

V4l2Capture::~V4l2Capture()
{
    close();
}

bool V4l2Capture::open()
{
    struct stat st;

    if (stat(m_device.toLocal8Bit().constData(), &st) == -1)
    {
        qWarning("V4L2: device %s not found", qPrintable(m_device));
        return false;
    }

    m_fd = ::open(m_device.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0)
    {
        qWarning("V4L2: cannot open %s: %s", qPrintable(m_device),
                 strerror(errno));
        return false;
    }

    if (!initDevice())
        return false;

    if (!setFormat())
        return false;

    if (!initMmap())
        return false;

    return true;
}

void V4l2Capture::close()
{
    stop();

    if (m_buffers)
    {
        for (unsigned int i = 0; i < m_nbufs; i++)
        {
            if (m_buffers[i].start)
                munmap(m_buffers[i].start, m_buffers[i].length);
        }
        free(m_buffers);
        m_buffers = nullptr;
    }

    if (m_fd >= 0)
    {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool V4l2Capture::initDevice()
{
    struct v4l2_capability cap;

    if (ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        qWarning("V4L2: VIDIOC_QUERYCAP failed: %s", strerror(errno));
        return false;
    }

    qDebug("V4L2: driver='%s' card='%s' bus='%s' version=%u.%u.%u",
           cap.driver, cap.card, cap.bus_info,
           (cap.version >> 16) & 0xFF,
           (cap.version >> 8) & 0xFF,
           cap.version & 0xFF);
    qDebug("V4L2: capabilities=0x%08X", cap.capabilities);
    qDebug("V4L2:   V4L2_CAP_VIDEO_CAPTURE=%s",
           (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ? "YES" : "NO");
    qDebug("V4L2:   V4L2_CAP_STREAMING=%s",
           (cap.capabilities & V4L2_CAP_STREAMING) ? "YES" : "NO");
    qDebug("V4L2:   V4L2_CAP_VIDEO_OUTPUT=%s",
           (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) ? "YES" : "NO");
    qDebug("V4L2:   V4L2_CAP_VIDEO_OVERLAY=%s",
           (cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) ? "YES" : "NO");
    qDebug("V4L2:   V4L2_CAP_READWRITE=%s",
           (cap.capabilities & V4L2_CAP_READWRITE) ? "YES" : "NO");
    qDebug("V4L2:   V4L2_CAP_DEVICE_CAPS=%s",
           (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? "YES" : "NO");

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        qWarning("V4L2: device does not support video capture");
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        qWarning("V4L2: device does not support streaming");
        return false;
    }

    return true;
}

bool V4l2Capture::setFormat()
{
    struct v4l2_format fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = m_width;
    fmt.fmt.pix.height      = m_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        qWarning("V4L2: VIDIOC_S_FMT failed: %s", strerror(errno));
        return false;
    }

    m_width  = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;

    qDebug("V4L2: format set to %dx%d (YUYV)", m_width, m_height);

    return true;
}

bool V4l2Capture::initMmap()
{
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof(req));
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        qWarning("V4L2: VIDIOC_REQBUFS failed: %s", strerror(errno));
        return false;
    }

    if (req.count < 2)
    {
        qWarning("V4L2: insufficient buffer memory");
        return false;
    }

    m_buffers = (v4l2_buffer_info *)calloc(req.count, sizeof(*m_buffers));
    if (!m_buffers)
    {
        qWarning("V4L2: out of memory");
        return false;
    }

    for (m_nbufs = 0; m_nbufs < req.count; m_nbufs++)
    {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = m_nbufs;

        if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            qWarning("V4L2: VIDIOC_QUERYBUF failed: %s", strerror(errno));
            return false;
        }

        m_buffers[m_nbufs].length = buf.length;
        m_buffers[m_nbufs].start  = mmap(NULL, buf.length,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED, m_fd, buf.m.offset);

        if (m_buffers[m_nbufs].start == MAP_FAILED)
        {
            qWarning("V4L2: mmap failed: %s", strerror(errno));
            return false;
        }
    }

    return true;
}

bool V4l2Capture::start()
{
    for (unsigned int i = 0; i < m_nbufs; i++)
    {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0)
        {
            qWarning("V4L2: VIDIOC_QBUF failed: %s", strerror(errno));
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_STREAMON, &type) < 0)
    {
        qWarning("V4L2: VIDIOC_STREAMON failed: %s", strerror(errno));
        return false;
    }

    m_running = true;
    qDebug("V4L2: streaming started");
    return true;
}

void V4l2Capture::stop()
{
    if (!m_running)
        return;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(m_fd, VIDIOC_STREAMOFF, &type);
    m_running = false;
}

QImage V4l2Capture::grabFrame()
{
    if (!m_running)
        return QImage();

    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0)
        return QImage();

    if (m_frame.isNull())
        m_frame = QImage(m_width, m_height, QImage::Format_RGB32);

    yuyvToRgb32((unsigned char *)m_buffers[buf.index].start,
                m_frame.bits(), m_width, m_height);

    if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0)
        qWarning("V4L2: VIDIOC_QBUF failed: %s", strerror(errno));

    return m_frame;
}

void V4l2Capture::yuyvToRgb32(const unsigned char *yuyv, unsigned char *rgb,
                               int width, int height)
{
    int total = width * height;
    int i = 0;

#ifdef __ARM_NEON__
    for (; i <= total - 8; i += 8) {
        uint8x8x2_t yuyv_pair = vld2_u8(yuyv + i * 2);
        uint8x8_t y = yuyv_pair.val[0];
        uint8x8_t uv = yuyv_pair.val[1];

        uint8x8x2_t uv_deint = vuzp_u8(uv, uv);
        uint8x8_t u4 = uv_deint.val[0];
        uint8x8_t v4 = uv_deint.val[1];

        uint8x8x2_t u8 = vzip_u8(u4, u4);
        uint8x8x2_t v8 = vzip_u8(v4, v4);

        int16x8_t y16  = vreinterpretq_s16_u16(vmovl_u8(y));
        int16x8_t u16  = vreinterpretq_s16_u16(vmovl_u8(u8.val[0]));
        int16x8_t v16  = vreinterpretq_s16_u16(vmovl_u8(v8.val[0]));

        int16x8_t u_sub = vsubq_s16(u16, vdupq_n_s16(128));
        int16x8_t v_sub = vsubq_s16(v16, vdupq_n_s16(128));

        int16x8_t rv = vshrq_n_s16(vmulq_s16(v_sub, vdupq_n_s16(359)), 8);
        int16x8_t gu = vshrq_n_s16(vmulq_s16(u_sub, vdupq_n_s16(88)), 8);
        int16x8_t gv = vshrq_n_s16(vmulq_s16(v_sub, vdupq_n_s16(183)), 8);
        int16x8_t bu = vshrq_n_s16(vmulq_s16(u_sub, vdupq_n_s16(454)), 8);

        int16x8_t r = vaddq_s16(y16, rv);
        int16x8_t g = vsubq_s16(vsubq_s16(y16, gu), gv);
        int16x8_t b = vaddq_s16(y16, bu);

        uint8x8_t r_u8 = vqmovun_s16(r);
        uint8x8_t g_u8 = vqmovun_s16(g);
        uint8x8_t b_u8 = vqmovun_s16(b);
        uint8x8_t a_u8 = vdup_n_u8(0xff);

        uint8x8x4_t rgba = { { b_u8, g_u8, r_u8, a_u8 } };
        vst4_u8(rgb + i * 4, rgba);
    }
#endif

    for (; i < total; i += 2) {
        int y0 = yuyv[i * 2];
        int u  = yuyv[i * 2 + 1] - 128;
        int y1 = yuyv[i * 2 + 2];
        int v  = yuyv[i * 2 + 3] - 128;

        int rv = (v * 359) >> 8;
        int gu = (u * 88) >> 8;
        int gv = (v * 183) >> 8;
        int bu = (u * 454) >> 8;

        int r0 = y0 + rv; if (r0 < 0) r0 = 0; if (r0 > 255) r0 = 255;
        int g0 = y0 - gu - gv; if (g0 < 0) g0 = 0; if (g0 > 255) g0 = 255;
        int b0 = y0 + bu; if (b0 < 0) b0 = 0; if (b0 > 255) b0 = 255;

        int r1 = y1 + rv; if (r1 < 0) r1 = 0; if (r1 > 255) r1 = 255;
        int g1 = y1 - gu - gv; if (g1 < 0) g1 = 0; if (g1 > 255) g1 = 255;
        int b1 = y1 + bu; if (b1 < 0) b1 = 0; if (b1 > 255) b1 = 255;

        rgb[i * 4 + 0] = b0;
        rgb[i * 4 + 1] = g0;
        rgb[i * 4 + 2] = r0;
        rgb[i * 4 + 3] = 0xff;

        rgb[(i + 1) * 4 + 0] = b1;
        rgb[(i + 1) * 4 + 1] = g1;
        rgb[(i + 1) * 4 + 2] = r1;
        rgb[(i + 1) * 4 + 3] = 0xff;
    }
}

bool V4l2Capture::queryControl(uint32_t ctrl_id, int *min, int *max, int *step, int *def)
{
    struct v4l2_queryctrl qctrl;
    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = ctrl_id;

    if (ioctl(m_fd, VIDIOC_QUERYCTRL, &qctrl) < 0) {
        qWarning("V4L2: VIDIOC_QUERYCTRL(0x%08X) failed: %s", ctrl_id, strerror(errno));
        return false;
    }

    if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        qWarning("V4L2: control 0x%08X is disabled", ctrl_id);
        return false;
    }

    *min  = qctrl.minimum;
    *max  = qctrl.maximum;
    *step = qctrl.step ? qctrl.step : 1;
    *def  = qctrl.default_value;
    return true;
}

bool V4l2Capture::setControl(uint32_t ctrl_id, int value)
{
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id    = ctrl_id;
    ctrl.value = value;

    if (ioctl(m_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        qWarning("V4L2: VIDIOC_S_CTRL(0x%08X, %d) failed: %s",
                 ctrl_id, value, strerror(errno));
        return false;
    }

    return true;
}

int V4l2Capture::getControl(uint32_t ctrl_id)
{
    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = ctrl_id;

    if (ioctl(m_fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        qWarning("V4L2: VIDIOC_G_CTRL(0x%08X) failed: %s", ctrl_id, strerror(errno));
        return -1;
    }

    return ctrl.value;
}