#ifndef MAINPAGE_H
#define MAINPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QProcess>
#include <QLabel>
#include <QTimer>
#include <QJsonObject>

class BacklightPanel;
class TouchGestureDetector;
class NetworkKeyboard;

class AppCard : public QPushButton
{
    Q_OBJECT
public:
    explicit AppCard(const QString &name, const QColor &color,
                     int iconType, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_name;
    QColor  m_color;
    int     m_iconType;
};

class MainPage : public QWidget
{
    Q_OBJECT
public:
    explicit MainPage(QWidget *parent = nullptr);
    ~MainPage();

protected:
    void resizeEvent(QResizeEvent *) override;

private slots:
    void onAppClicked(int index);
    void onAppFinished(int exitCode);
    void updateClock();
    void onBrightnessChanged(int level);
    void onNetworkConnected();
    void onRemoteCommand(const QString &cmd, const QJsonObject &params);
    void publishDeviceStatus();
    void onThreeFingerSwipeUp();

private:
    int  readBacklight();
    void writeBacklight(int level);
    void stopCurrentApp();
    int  appIndexFromName(const QString &name);

    QProcess             *m_process;
    QLabel               *m_clockLabel;
    QTimer               *m_clockTimer;
    bool                  m_appRunning;

    BacklightPanel       *m_backlightPanel;
    TouchGestureDetector *m_gestureDetector;
    NetworkKeyboard      *m_keyboard;

    QLabel               *m_netStatusLabel;
    QTimer               *m_statusTimer;

    static const int kBacklightLevels[8];
};

#endif