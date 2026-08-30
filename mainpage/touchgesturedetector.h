#ifndef TOUCHGESTUREDETECTOR_H
#define TOUCHGESTUREDETECTOR_H

#include <QObject>
#include <QTimer>
#include <linux/input.h>

class TouchGestureDetector : public QObject
{
    Q_OBJECT
public:
    explicit TouchGestureDetector(QObject *parent = nullptr);
    ~TouchGestureDetector();

    void start();
    void stop();
    bool isActive() const { return m_touchFd >= 0; }

signals:
    void threeFingerSwipeDown();
    void threeFingerSwipeUp();

private slots:
    void checkTouchGesture();

private:
    int  findTouchDevice();

    QTimer *m_timer;
    int     m_touchFd;

    struct TouchPoint
    {
        int id;
        int x;
        int y;
        int startY;
        bool active;
    };
    TouchPoint m_touchPoints[10];
    bool       m_gestureTracking;
    bool       m_gestureTriggered;
    bool       m_gestureTriggeredUp;
};

#endif