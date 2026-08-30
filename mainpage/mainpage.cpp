#include "mainpage.h"
#include "backlightpanel.h"
#include "touchgesturedetector.h"
#include "settingsmanager.h"
#include "networkmanager.h"
#include "networkkeyboard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontDatabase>
#include <QDateTime>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QDialog>
#include <QFrame>
#include <QLineEdit>
#include <QIntValidator>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <cmath>

AppCard::AppCard(const QString &name, const QColor &color,
                 int iconType, QWidget *parent)
    : QPushButton(parent), m_name(name), m_color(color), m_iconType(iconType)
{
    setFixedSize(150, 170);
    setCursor(Qt::PointingHandCursor);
}

void AppCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width(), h = height();
    int centerX = w / 2;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(50, 50, 55));
    p.drawRoundedRect(2, 2, w - 4, h - 4, 14, 14);

    int iconR = 28;
    int iconY = 20;
    QRect iconRect(centerX - iconR, iconY, iconR * 2, iconR * 2);
    p.setBrush(m_color);
    p.drawEllipse(iconRect);

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);

    if (m_iconType == 0)
    {
        int bx = centerX - 16, by = iconY + 12;
        p.drawRoundedRect(bx, by, 32, 24, 4, 4);
        p.setBrush(m_color);
        p.drawRoundedRect(bx + 2, by + 2, 28, 20, 3, 3);
        p.setBrush(Qt::white);
        p.drawEllipse(QPoint(centerX, by + 22), 6, 6);
        p.setBrush(m_color);
        p.drawEllipse(QPoint(centerX, by + 22), 4, 4);
        p.setBrush(QColor(60, 60, 65));
        p.drawRoundedRect(centerX - 3, by - 3, 6, 5, 2, 2);
    }
    else if (m_iconType == 1)
    {
        int mx = centerX, my = iconY + 14;
        QPainterPath note;
        note.addEllipse(QPoint(mx - 7, my + 12), 6, 5);
        note.addRect(mx + 1, my - 5, 4, 18);
        note.addEllipse(QPoint(mx + 5, my - 5), 3, 2);
        p.drawPath(note);
    }
    else if (m_iconType == 2)
    {
        int sx = centerX, sy = iconY + 14;
        QPainterPath ship;
        ship.moveTo(sx, sy - 2);
        ship.lineTo(sx - 12, sy + 18);
        ship.lineTo(sx - 4, sy + 10);
        ship.lineTo(sx, sy + 22);
        ship.lineTo(sx + 4, sy + 10);
        ship.lineTo(sx + 12, sy + 18);
        ship.closeSubpath();
        p.drawPath(ship);
        p.setBrush(m_color);
        p.drawEllipse(QPoint(sx, sy + 4), 3, 3);
    }
    else if (m_iconType == 3)
    {
        int fx = centerX, fy = iconY + 10;
        p.drawRoundedRect(fx - 16, fy + 6, 32, 22, 4, 4);
        p.setBrush(m_color);
        p.drawRoundedRect(fx - 16, fy, 14, 6, 3, 3);
        p.setBrush(Qt::white);
        p.drawRoundedRect(fx - 14, fy + 6, 28, 2, 1, 1);
        p.drawRoundedRect(fx - 12, fy + 10, 18, 2, 1, 1);
        p.drawRoundedRect(fx - 10, fy + 14, 14, 2, 1, 1);
    }
    else if (m_iconType == 4)
    {
        int tx = centerX, ty = iconY + 8;
        p.setBrush(m_color);
        p.drawRoundedRect(tx - 16, ty, 32, 24, 4, 4);
        p.setBrush(QColor(20, 20, 30));
        p.drawRoundedRect(tx - 13, ty + 3, 26, 18, 2, 2);
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(tx - 9, ty + 16, tx - 2, ty + 16);
        p.drawLine(tx - 9, ty + 16, tx - 6, ty + 20);
        p.drawLine(tx - 9, ty + 16, tx - 6, ty + 12);
        p.setPen(Qt::NoPen);
    }
    else
    {
        int gx = centerX, gy = iconY + 14;
        p.setBrush(m_color);
        p.drawRoundedRect(gx - 14, gy - 8, 28, 28, 8, 8);
        p.setBrush(QColor(20, 20, 30));
        p.drawEllipse(QPoint(gx, gy), 10, 10);
        p.setBrush(m_color);
        p.drawRect(gx - 2, gy - 14, 4, 8);
        p.drawRect(gx - 2, gy + 6, 4, 8);
        p.drawRect(gx - 14, gy - 2, 8, 4);
        p.drawRect(gx + 6, gy - 2, 8, 4);
        p.setBrush(Qt::white);
        p.drawEllipse(QPoint(gx, gy), 4, 4);
    }

    p.setPen(Qt::white);
    QFont nameFont("WenQuanYi Zen Hei", 16, QFont::Bold);
    p.setFont(nameFont);
    p.drawText(QRect(0, 125, w, 36), Qt::AlignHCenter, m_name);
}

const int MainPage::kBacklightLevels[8] = {0, 1, 2, 3, 4, 5, 6, 7};

int MainPage::readBacklight()
{
    QFile f("/sys/class/backlight/backlight/brightness");
    if (!f.open(QIODevice::ReadOnly))
    {
        qDebug() << "BACKLIGHT: 无法读取 /sys/class/backlight/backlight/brightness";
        return 7;
    }
    QString val = QString::fromLocal8Bit(f.readAll().trimmed());
    int raw = val.toInt();
    for (int i = 0; i < 8; i++)
    {
        if (kBacklightLevels[i] >= raw)
        {
            return i;
        }
    }
    return 7;
}

void MainPage::writeBacklight(int level)
{
    if (level < 0)
    {
        level = 0;
    }
    if (level > 7)
    {
        level = 7;
    }
    int raw = kBacklightLevels[level];
    qDebug() << "BACKLIGHT: 写入 level=" << level << "raw=" << raw;
    QFile f("/sys/class/backlight/backlight/brightness");
    if (f.open(QIODevice::WriteOnly))
    {
        f.write(QByteArray::number(raw));
        f.close();
        qDebug() << "BACKLIGHT: 写入成功";
    }
    else
    {
        qDebug() << "BACKLIGHT: 写入失败（无权限或路径不存在）";
    }
}

void MainPage::onBrightnessChanged(int level)
{
    writeBacklight(level);
}

MainPage::MainPage(QWidget *parent)
    : QWidget(parent), m_process(nullptr), m_appRunning(false)
{
    setWindowTitle("智能设备");

    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_clockLabel = new QLabel(this);
    m_clockLabel->setAlignment(Qt::AlignCenter);
    m_clockLabel->setFont(QFont("WenQuanYi Zen Hei", 22, QFont::Bold));
    m_clockLabel->setStyleSheet("color: white; padding: 16px 0;");
    m_clockLabel->setText(QDateTime::currentDateTime().toString("HH:mm"));

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainPage::updateClock);
    m_clockTimer->start(10000);
    mainLayout->addWidget(m_clockLabel);

    mainLayout->addStretch(1);

    QHBoxLayout *cardRow = new QHBoxLayout;
    cardRow->setSpacing(24);
    cardRow->addStretch();

    AppCard *cameraCard = new AppCard("相机", QColor(76, 175, 80), 0, this);
    connect(cameraCard, &QPushButton::clicked, this, [this]() { onAppClicked(0); });
    cardRow->addWidget(cameraCard);

    AppCard *audioCard = new AppCard("音乐", QColor(33, 150, 243), 1, this);
    connect(audioCard, &QPushButton::clicked, this, [this]() { onAppClicked(1); });
    cardRow->addWidget(audioCard);

    AppCard *gameCard = new AppCard("游戏", QColor(255, 152, 0), 2, this);
    connect(gameCard, &QPushButton::clicked, this, [this]() { onAppClicked(2); });
    cardRow->addWidget(gameCard);

    AppCard *fileCard = new AppCard("文件", QColor(156, 39, 176), 3, this);
    connect(fileCard, &QPushButton::clicked, this, [this]() { onAppClicked(3); });
    cardRow->addWidget(fileCard);

    AppCard *termCard = new AppCard("终端", QColor(0, 200, 83), 4, this);
    connect(termCard, &QPushButton::clicked, this, [this]() { onAppClicked(4); });
    cardRow->addWidget(termCard);

    AppCard *netCard = new AppCard("网络", QColor(255, 87, 34), 5, this);
    connect(netCard, &QPushButton::clicked, this, [this]() { onAppClicked(5); });
    cardRow->addWidget(netCard);

    cardRow->addStretch();
    mainLayout->addLayout(cardRow);

    mainLayout->addStretch(1);

    m_netStatusLabel = new QLabel("● 离线", this);
    m_netStatusLabel->setAlignment(Qt::AlignCenter);
    m_netStatusLabel->setFont(QFont("WenQuanYi Zen Hei", 10));
    m_netStatusLabel->setStyleSheet("color: #f44336; padding: 4px;");
    mainLayout->addWidget(m_netStatusLabel);

    mainLayout->addStretch(1);

    QLabel *verLabel = new QLabel("i.MX6ULL Embedded System v1.0", this);
    verLabel->setAlignment(Qt::AlignCenter);
    verLabel->setFont(QFont("WenQuanYi Zen Hei", 10));
    verLabel->setStyleSheet("color: #666; padding: 8px;");
    mainLayout->addWidget(verLabel);

    setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                  "stop:0 #1a1a2e, stop:1 #16213e);");

    m_backlightPanel = new BacklightPanel(this);
    connect(m_backlightPanel, &BacklightPanel::brightnessChanged,
            this, &MainPage::onBrightnessChanged);

    int curLevel = readBacklight();
    m_backlightPanel->setValue(curLevel);

    m_gestureDetector = new TouchGestureDetector(this);
    connect(m_gestureDetector, &TouchGestureDetector::threeFingerSwipeDown,
            m_backlightPanel, &BacklightPanel::showPanel);
    connect(m_gestureDetector, &TouchGestureDetector::threeFingerSwipeUp,
            this, &MainPage::onThreeFingerSwipeUp);
    m_gestureDetector->start();

    m_keyboard = new NetworkKeyboard(this);

    NetworkManager *net = NetworkManager::instance();
    connect(net, &NetworkManager::connected, this, &MainPage::onNetworkConnected);
    connect(net, &NetworkManager::remoteCommand, this, &MainPage::onRemoteCommand);
    net->start();

    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(10000);
    connect(m_statusTimer, &QTimer::timeout, this, &MainPage::publishDeviceStatus);
    m_statusTimer->start();
}

void MainPage::resizeEvent(QResizeEvent *)
{
    m_keyboard->setFixedWidth(width());
}

MainPage::~MainPage()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
        m_process->waitForFinished(3000);
    }
}

void MainPage::updateClock()
{
    m_clockLabel->setText(QDateTime::currentDateTime().toString("HH:mm"));
}

void MainPage::onAppClicked(int index)
{
    if (m_appRunning)
    {
        return;
    }

    if (!m_process)
    {
        m_process = new QProcess(this);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) { onAppFinished(code); });
    }

    QStringList args;
    args << "-platform" << "linuxfb";

    if (index == 0)
    {
        NetworkManager::instance()->stopStream();
        m_process->setWorkingDirectory("/usr/bin");
        m_process->start("/usr/bin/camera", args);
    }
    else if (index == 1)
    {
        m_process->setWorkingDirectory("/usr/bin");
        m_process->start("/usr/bin/audio", args);
    }
    else if (index == 2)
    {
        m_process->setWorkingDirectory("/usr/bin");
        m_process->start("/usr/bin/spaceship", args);
    }
    else if (index == 3)
    {
        m_process->setWorkingDirectory("/usr/bin");
        m_process->start("/usr/bin/filemanager", args);
    }
    else if (index == 4)
    {
        m_process->setWorkingDirectory("/usr/bin");
        m_process->start("/usr/bin/terminal", args);
    }
    else
    {
        m_process->setWorkingDirectory("/usr/bin");
        m_process->start("/usr/bin/network", args);
    }

    if (m_process->waitForStarted(3000))
    {
        m_appRunning = true;
        NetworkManager::instance()->setAppRunning(true);
        hide();
    }
    else
    {
        qWarning() << "Failed to start app";
    }
}

void MainPage::onAppFinished(int exitCode)
{
    Q_UNUSED(exitCode);
    m_appRunning = false;
    NetworkManager::instance()->setAppRunning(false);
    show();
    raise();
}

void MainPage::onNetworkConnected()
{
    m_netStatusLabel->setText("● 在线");
    m_netStatusLabel->setStyleSheet("color: #4CAF50; padding: 4px;");
}

void MainPage::onRemoteCommand(const QString &cmd, const QJsonObject &params)
{
    if (cmd == "set_brightness" || cmd.endsWith("/set_brightness"))
    {
        int level = params["level"].toInt();
        m_backlightPanel->setValue(level);
        writeBacklight(level);
    }
    else if (cmd == "capture" || cmd.endsWith("/capture"))
    {
        onAppClicked(0);
    }
    else if (cmd == "launch_app" || cmd.endsWith("/launch_app"))
    {
        QString name = params["name"].toString();
        int index = appIndexFromName(name);
        if (index >= 0)
        {
            onAppClicked(index);
        }
        else
        {
            qWarning() << "REMOTE: 未知应用名:" << name;
        }
    }
    else if (cmd == "stop_app" || cmd.endsWith("/stop_app"))
    {
        stopCurrentApp();
    }
    else if (cmd == "start_stream" || cmd.endsWith("/start_stream"))
    {
        NetworkManager::instance()->startStream();
    }
    else if (cmd == "stop_stream" || cmd.endsWith("/stop_stream"))
    {
        NetworkManager::instance()->stopStream();
    }
    else if (cmd == "get_status" || cmd.endsWith("/get_status"))
    {
        publishDeviceStatus();
    }
    else if (cmd == "reboot" || cmd.endsWith("/reboot"))
    {
        int ret = system("reboot");
        Q_UNUSED(ret);
    }
    else if (cmd == "shutdown" || cmd.endsWith("/shutdown"))
    {
        int ret = system("shutdown -h now");
        Q_UNUSED(ret);
    }
}

int MainPage::appIndexFromName(const QString &name)
{
    if (name == "camera")
    {
        return 0;
    }
    else if (name == "audio")
    {
        return 1;
    }
    else if (name == "game")
    {
        return 2;
    }
    else if (name == "filemanager")
    {
        return 3;
    }
    else if (name == "terminal")
    {
        return 4;
    }
    else if (name == "network")
    {
        return 5;
    }
    return -1;
}

void MainPage::stopCurrentApp()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
    {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
        {
            m_process->kill();
        }
    }
    m_appRunning = false;
    NetworkManager::instance()->setAppRunning(false);
    show();
    raise();
}

void MainPage::publishDeviceStatus()
{
    QJsonObject status;
    status["brightness"] = readBacklight();
    status["appRunning"] = m_appRunning;
    status["streaming"] = NetworkManager::instance()->isStreaming();

    QFile tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (tempFile.open(QIODevice::ReadOnly))
    {
        int temp = tempFile.readAll().trimmed().toInt() / 1000;
        status["cpuTemp"] = temp;
    }

    NetworkManager::instance()->publishStatus(status);
}

void MainPage::onThreeFingerSwipeUp()
{
    QDateTime now = QDateTime::currentDateTime();

    QDialog dlg(this);
    dlg.setWindowTitle("设置时间");
    dlg.setFixedSize(480, 480);
    dlg.setStyleSheet(
        "QDialog { background: #1e1e30; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    QLabel *titleLabel = new QLabel("设置时间");
    titleLabel->setStyleSheet("color: #fff; font-size: 18px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QHBoxLayout *timeRow = new QHBoxLayout;
    QLabel *timeLabel = new QLabel("时间");
    timeLabel->setStyleSheet("color: #ccc; font-size: 15px;");
    QLineEdit *hourBox = new QLineEdit(&dlg);
    hourBox->setValidator(new QIntValidator(0, 23, hourBox));
    hourBox->setText(QString::number(now.time().hour()));
    hourBox->setAlignment(Qt::AlignCenter);
    hourBox->setStyleSheet("background: rgba(255,255,255,0.08); color: #fff; border: 1px solid rgba(255,255,255,0.15);"
        " border-radius: 6px; padding: 8px; font-size: 20px; min-width: 60px;");
    QLineEdit *minBox = new QLineEdit(&dlg);
    minBox->setValidator(new QIntValidator(0, 59, minBox));
    minBox->setText(QString::number(now.time().minute()));
    minBox->setAlignment(Qt::AlignCenter);
    minBox->setStyleSheet(hourBox->styleSheet());
    timeRow->addWidget(timeLabel);
    timeRow->addStretch();
    timeRow->addWidget(hourBox);
    timeRow->addWidget(new QLabel("时"));
    timeRow->addWidget(minBox);
    timeRow->addWidget(new QLabel("分"));
    mainLayout->addLayout(timeRow);

    QHBoxLayout *dateRow = new QHBoxLayout;
    QLabel *dateLabel = new QLabel("日期");
    dateLabel->setStyleSheet("color: #ccc; font-size: 15px;");
    QLineEdit *yearBox = new QLineEdit(&dlg);
    yearBox->setValidator(new QIntValidator(2024, 2099, yearBox));
    yearBox->setText(QString::number(now.date().year()));
    yearBox->setAlignment(Qt::AlignCenter);
    yearBox->setStyleSheet("background: rgba(255,255,255,0.08); color: #fff; border: 1px solid rgba(255,255,255,0.15);"
        " border-radius: 6px; padding: 8px; font-size: 20px; min-width: 72px;");
    QLineEdit *monthBox = new QLineEdit(&dlg);
    monthBox->setValidator(new QIntValidator(1, 12, monthBox));
    monthBox->setText(QString::number(now.date().month()));
    monthBox->setAlignment(Qt::AlignCenter);
    monthBox->setStyleSheet(yearBox->styleSheet());
    QLineEdit *dayBox = new QLineEdit(&dlg);
    dayBox->setValidator(new QIntValidator(1, 31, dayBox));
    dayBox->setText(QString::number(now.date().day()));
    dayBox->setAlignment(Qt::AlignCenter);
    dayBox->setStyleSheet(yearBox->styleSheet());
    dateRow->addWidget(dateLabel);
    dateRow->addStretch();
    dateRow->addWidget(yearBox);
    dateRow->addWidget(new QLabel("年"));
    dateRow->addWidget(monthBox);
    dateRow->addWidget(new QLabel("月"));
    dateRow->addWidget(dayBox);
    dateRow->addWidget(new QLabel("日"));
    mainLayout->addLayout(dateRow);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton("取消");
    cancelBtn->setStyleSheet("background: rgba(255,255,255,0.08); color: #fff; border: none;"
        " border-radius: 8px; padding: 10px 24px; font-size: 15px;");
    QPushButton *okBtn = new QPushButton("确定");
    okBtn->setStyleSheet("background: #4CAF50; color: #fff; border: none;"
        " border-radius: 8px; padding: 10px 24px; font-size: 15px;");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    mainLayout->addLayout(btnRow);

    QFrame *sep = new QFrame(&dlg);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #333;");
    mainLayout->addWidget(sep);

    QWidget *kb = new QWidget(&dlg);
    QGridLayout *grid = new QGridLayout(kb);
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    for (int c = 0; c < 10; c++)
        grid->setColumnStretch(c, 1);

    auto mkKey = [&](const QString &t, int h) {
        QPushButton *b = new QPushButton(t, kb);
        b->setFixedHeight(h);
        b->setFont(QFont("WenQuanYi Zen Hei", 13, QFont::Bold));
        b->setFocusPolicy(Qt::NoFocus);
        b->setStyleSheet("QPushButton { background: #2a2a3e; color: #ddd; border: 1px solid #3a3a4e;"
            " border-radius: 6px; } QPushButton:pressed { background: #4a4a6e; color: #fff; }");
        return b;
    };

    const int KH = 44;
    int r = 0;

    QStringList nums = {"1","2","3","4","5","6","7","8","9","0"};
    for (int c = 0; c < 10; c++) {
        QPushButton *b = mkKey(nums[c], KH);
        connect(b, &QPushButton::clicked, &dlg, [&dlg, t = nums[c]]() {
            QWidget *w = QApplication::focusWidget();
            QLineEdit *e = qobject_cast<QLineEdit *>(w);
            if (e) e->insert(t);
        });
        grid->addWidget(b, r, c);
    }
    r++;

    QStringList letters = {"q","w","e","r","t","y","u","i","o","p"};
    for (int c = 0; c < 10; c++) {
        QPushButton *b = mkKey(letters[c], KH);
        connect(b, &QPushButton::clicked, &dlg, [&dlg, t = letters[c]]() {
            QWidget *w = QApplication::focusWidget();
            QLineEdit *e = qobject_cast<QLineEdit *>(w);
            if (e) e->insert(t);
        });
        grid->addWidget(b, r, c);
    }
    r++;

    for (int c = 0; c < 9; c++) {
        QString t = QString(QChar('a' + c));
        QPushButton *b = mkKey(t, KH);
        connect(b, &QPushButton::clicked, &dlg, [&dlg, t]() {
            QWidget *w = QApplication::focusWidget();
            QLineEdit *e = qobject_cast<QLineEdit *>(w);
            if (e) e->insert(t);
        });
        grid->addWidget(b, r, c + 1);
    }
    r++;

    QPushButton *shiftBtn = mkKey("Shift", 48);
    shiftBtn->setStyleSheet("QPushButton { background: #3a3a50; color: #FFD54F; border: 1px solid #4a4a60;"
        " border-radius: 6px; font-size: 12px; } QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }");
    grid->addWidget(shiftBtn, r, 0, 1, 2);

    QStringList row4 = {"z","x","c","v","b","n","m"};
    for (int c = 0; c < 7; c++) {
        QPushButton *b = mkKey(row4[c], KH);
        connect(b, &QPushButton::clicked, &dlg, [&dlg, t = row4[c]]() {
            QWidget *w = QApplication::focusWidget();
            QLineEdit *e = qobject_cast<QLineEdit *>(w);
            if (e) e->insert(t);
        });
        grid->addWidget(b, r, c + 2);
    }

    QPushButton *bsBtn = mkKey("⌫", 48);
    bsBtn->setStyleSheet("QPushButton { background: #3a3a50; color: #ff6666; border: 1px solid #4a4a60;"
        " border-radius: 6px; } QPushButton:pressed { background: #ff4444; color: #fff; }");
    connect(bsBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->backspace();
    });
    grid->addWidget(bsBtn, r, 9);
    r++;

    QPushButton *dashBtn = mkKey("-", 48);
    connect(dashBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->insert("-");
    });
    grid->addWidget(dashBtn, r, 0);

    QPushButton *slashBtn = mkKey("/", 48);
    connect(slashBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->insert("/");
    });
    grid->addWidget(slashBtn, r, 1);

    QPushButton *spaceBtn = mkKey("Space", 48);
    connect(spaceBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->insert(" ");
    });
    grid->addWidget(spaceBtn, r, 2, 1, 3);

    QPushButton *dotBtn = mkKey(".", 48);
    connect(dotBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->insert(".");
    });
    grid->addWidget(dotBtn, r, 5);

    QPushButton *colonBtn = mkKey(":", 48);
    connect(colonBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->insert(":");
    });
    grid->addWidget(colonBtn, r, 6);

    QPushButton *atBtn = mkKey("@", 48);
    connect(atBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->insert("@");
    });
    grid->addWidget(atBtn, r, 7);

    QPushButton *enterBtn = mkKey("Enter", 48);
    enterBtn->setStyleSheet("QPushButton { background: #2a5a2e; color: #00e676; border: 1px solid #3a7a3e;"
        " border-radius: 6px; font-size: 12px; } QPushButton:pressed { background: #00e676; color: #1a1a2e; }");
    connect(enterBtn, &QPushButton::clicked, &dlg, [&dlg]() {
        QWidget *w = QApplication::focusWidget();
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        if (e) e->clearFocus();
    });
    grid->addWidget(enterBtn, r, 8, 1, 2);

    mainLayout->addWidget(kb);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    hourBox->setFocus();

    if (dlg.exec() == QDialog::Accepted)
    {
        QString dateStr = QString("%1-%2-%3 %4:%5:00")
            .arg(yearBox->text().toInt(), 4, 10, QChar('0'))
            .arg(monthBox->text().toInt(), 2, 10, QChar('0'))
            .arg(dayBox->text().toInt(), 2, 10, QChar('0'))
            .arg(hourBox->text().toInt(), 2, 10, QChar('0'))
            .arg(minBox->text().toInt(), 2, 10, QChar('0'));

        QProcess proc;
        proc.start("date", QStringList() << "-s" << dateStr);
        if (proc.waitForFinished(3000))
        {
            QString hwClock = QString("/sbin/hwclock -w");
            int ret = system(hwClock.toLocal8Bit().constData());
            Q_UNUSED(ret);
            updateClock();
        }
    }
}