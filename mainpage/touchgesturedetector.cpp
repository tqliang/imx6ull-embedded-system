#include "touchgesturedetector.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <cstring>
#include <cerrno>
#include <climits>

TouchGestureDetector::TouchGestureDetector(QObject *parent)
    : QObject(parent)
    , m_timer(nullptr)
    , m_touchFd(-1)
    , m_gestureTracking(false)
    , m_gestureTriggered(false)
    , m_gestureTriggeredUp(false)
{
    memset(m_touchPoints, 0, sizeof(m_touchPoints));
}

TouchGestureDetector::~TouchGestureDetector()
{
    stop();
}

int TouchGestureDetector::findTouchDevice()
{
    DIR *dir = opendir("/dev/input");
    if (!dir)
    {
        return -1;
    }

    struct dirent *ent;
    char path[PATH_MAX];
    int bestFd = -1;

    while ((ent = readdir(dir)) != nullptr)
    {
        if (strncmp(ent->d_name, "event", 5) != 0)
        {
            continue;
        }

        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
        {
            continue;
        }

        unsigned long evBits[256 / 8] = {};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evBits)), evBits) >= 0)
        {
            if (!(evBits[0] & (1UL << EV_ABS)))
            {
                goto next;
            }

            evBits[0] = 0;
            if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(evBits)), evBits) >= 0)
            {
                if (evBits[0] != 0)
                {
                    goto next;
                }
            }

            qDebug() << "Touch device found:" << path;
            bestFd = fd;
            closedir(dir);
            return bestFd;
        }
next:
        ::close(fd);
    }
    closedir(dir);
    return -1;
}

void TouchGestureDetector::start()
{
    memset(m_touchPoints, 0, sizeof(m_touchPoints));
    m_gestureTracking = false;
    m_gestureTriggered = false;
    m_gestureTriggeredUp = false;

    m_touchFd = findTouchDevice();
    if (m_touchFd < 0)
    {
        qWarning() << "No multi-touch device found, gesture disabled";
        return;
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &TouchGestureDetector::checkTouchGesture);
    m_timer->start();

    qDebug() << "Touch gesture monitor started";
}

void TouchGestureDetector::stop()
{
    if (m_timer)
    {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
    if (m_touchFd >= 0)
    {
        ::close(m_touchFd);
        m_touchFd = -1;
    }
}

void TouchGestureDetector::checkTouchGesture()
{
    if (m_touchFd < 0)
    {
        return;
    }

    struct input_event ev;
    int currentSlot = 0;
    bool synReport = false;

    while (::read(m_touchFd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev))
    {
        if (ev.type == EV_ABS)
        {
            if (ev.code == ABS_MT_SLOT)
            {
                if (ev.value >= 0 && ev.value < 10)
                {
                    currentSlot = ev.value;
                }
            }
            else if (ev.code == ABS_MT_TRACKING_ID)
            {
                if (ev.value >= 0)
                {
                    m_touchPoints[currentSlot].id = ev.value;
                    m_touchPoints[currentSlot].active = true;
                }
                else
                {
                    m_touchPoints[currentSlot].active = false;
                    m_touchPoints[currentSlot].startY = 0;
                }
            }
            else if (ev.code == ABS_MT_POSITION_X)
            {
                m_touchPoints[currentSlot].x = ev.value;
            }
            else if (ev.code == ABS_MT_POSITION_Y)
            {
                m_touchPoints[currentSlot].y = ev.value;
            }
        }
        else if (ev.type == EV_SYN && ev.code == SYN_REPORT)
        {
            synReport = true;
            break;
        }
    }

    if (!synReport)
    {
        return;
    }

    int activeCount = 0;
    for (int i = 0; i < 10; i++)
    {
        if (m_touchPoints[i].active)
        {
            activeCount++;
        }
    }

    if (activeCount >= 3)
    {
        if (!m_gestureTracking)
        {
            m_gestureTracking = true;
            for (int i = 0; i < 10; i++)
            {
                if (m_touchPoints[i].active)
                {
                    m_touchPoints[i].startY = m_touchPoints[i].y;
                }
            }
        }

        if (!m_gestureTriggered)
        {
            bool allDown = true;
            bool allUp   = true;
            for (int i = 0; i < 10; i++)
            {
                if (m_touchPoints[i].active)
                {
                    if (m_touchPoints[i].y - m_touchPoints[i].startY < 80)
                    {
                        allDown = false;
                    }
                    if (m_touchPoints[i].startY - m_touchPoints[i].y < 80)
                    {
                        allUp = false;
                    }
                }
            }
            if (allDown)
            {
                m_gestureTriggered = true;
                emit threeFingerSwipeDown();
            }
            if (allUp && !m_gestureTriggeredUp)
            {
                m_gestureTriggeredUp = true;
                emit threeFingerSwipeUp();
            }
        }
    }
    else
    {
        m_gestureTracking = false;
        m_gestureTriggered = false;
        m_gestureTriggeredUp = false;
    }
}