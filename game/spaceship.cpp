#include "spaceship.h"
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QFont>
#include <QFontDatabase>
#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <linux/input.h>
#include <dirent.h>
#include <cstring>

ShipGame::ShipGame(QWidget *parent)
    : QWidget(parent), m_fd(-1), m_heading(0), m_shipX(0), m_shipY(0),
      m_lives(3), m_score(0), m_bestScore(0), m_difficulty(1),
      m_gameOver(false), m_paused(false), m_hasSensor(false),
      m_frameCount(0), m_starSeed(0), m_shootCooldown(0),
      m_spawnCounter(0), m_touchActive(false), m_touchShoot(false), m_touchX(0)
{
    setWindowTitle("太空飞船");

    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    srand(time(nullptr));
    m_starSeed = (float)rand() / RAND_MAX * 1000.0f;

    loadBestScore();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ShipGame::gameLoop);
    m_timer->start(33);

    m_elapsed.start();

    if (!openMagnetometer())
    {
        qWarning() << "MAG3110 not found, use keyboard arrows";
    }

    m_exitBtn = new QPushButton("X", this);
    m_exitBtn->setFixedSize(36, 36);
    m_exitBtn->setFont(QFont("WenQuanYi Zen Hei", 14, QFont::Bold));
    m_exitBtn->setStyleSheet(
        "QPushButton {"
        "  color: rgba(255,255,255,180);"
        "  background: rgba(0,0,0,120);"
        "  border: none;"
        "  border-radius: 18px;"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(255,80,80,180);"
        "}");
    m_exitBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exitBtn, &QPushButton::clicked, this, &QWidget::close);

    initGame();
    setStyleSheet("background: black;");
    setFocusPolicy(Qt::StrongFocus);
}

ShipGame::~ShipGame()
{
    if (m_fd >= 0)
    {
        ::close(m_fd);
    }
}

bool ShipGame::openMagnetometer()
{
    DIR *dir = opendir("/dev/input");
    if (!dir)
    {
        return false;
    }

    struct dirent *ent;
    char path[PATH_MAX];
    char name[256];

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

        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0)
        {
            if (strstr(name, "mag3110") != nullptr)
            {
                m_fd = fd;
                m_hasSensor = true;
                qDebug() << "MAG3110 opened:" << path;
                closedir(dir);
                return true;
            }
        }
        ::close(fd);
    }
    closedir(dir);
    return false;
}

void ShipGame::readMagnetometer()
{
    if (m_fd < 0)
    {
        return;
    }

    struct input_event ev;
    int x = 0, y = 0;
    bool updated = false;

    while (read(m_fd, &ev, sizeof(ev)) == sizeof(ev))
    {
        if (ev.type == EV_ABS)
        {
            if (ev.code == ABS_X)
            {
                x = ev.value;
                updated = true;
            }
            else if (ev.code == ABS_Y)
            {
                y = ev.value;
                updated = true;
            }
        }
    }

    if (updated)
    {
        float angle = atan2f((float)y, (float)x);
        m_heading = qRadiansToDegrees((double)angle);
    }
}

void ShipGame::initGame()
{
    m_shipX = width() / 2.0f;
    m_shipY = height() - 80.0f;
    m_lives = 3;
    m_score = 0;
    m_difficulty = 1;
    m_gameOver = false;
    m_paused = false;
    m_asteroids.clear();
    m_bullets.clear();
    m_particles.clear();
    m_scorePopups.clear();
    m_spawnCounter = 0;
    m_frameCount = 0;
    m_shootCooldown = 0;
    m_shakeFrame = 0;
}

void ShipGame::shoot()
{
    if (m_shootCooldown > 0)
    {
        return;
    }

    Bullet b;
    b.pos = QPointF(m_shipX, m_shipY - 24);
    b.speed = 16.0f;
    m_bullets.append(b);

    Bullet bl;
    bl.pos = QPointF(m_shipX - 8, m_shipY - 16);
    bl.speed = 16.0f;
    m_bullets.append(bl);

    Bullet br;
    br.pos = QPointF(m_shipX + 8, m_shipY - 16);
    br.speed = 16.0f;
    m_bullets.append(br);

    m_shootCooldown = 12;
}

void ShipGame::spawnExplosion(const QPointF &pos, const QColor &color, int count)
{
    for (int i = 0; i < count; i++)
    {
        Particle p;
        p.pos = pos;
        float angle = (float)(rand() % 360) * M_PI / 180.0f;
        float speed = 1.0f + (float)(rand() % 100) / 100.0f * 3.0f;
        p.vel = QPointF(cosf(angle) * speed, sinf(angle) * speed);
        p.life = 20.0f + (float)(rand() % 20);
        p.maxLife = p.life;
        p.color = color;
        p.size = 2.0f + (float)(rand() % 4);
        m_particles.append(p);
    }
}

void ShipGame::gameLoop()
{
    readMagnetometer();

    if (m_gameOver || m_paused)
    {
        update();
        return;
    }

    m_frameCount++;

    if (m_shootCooldown > 0)
    {
        m_shootCooldown--;
    }

    if (m_shakeFrame > 0)
    {
        m_shakeFrame--;
    }

    if (m_touchShoot && m_shootCooldown <= 0)
    {
        shoot();
    }

    int newDifficulty = m_score / 500 + 1;
    if (newDifficulty > m_difficulty)
    {
        m_difficulty = newDifficulty;
    }

    if (m_fd >= 0)
    {
        float targetX = width() / 2.0f + m_heading * 3.0f;
        m_shipX += (targetX - m_shipX) * 0.3f;
    }
    else if (m_touchActive)
    {
        m_shipX += (m_touchX - m_shipX) * 0.4f;
    }

    m_shipX = qBound(30.0f, m_shipX, (float)width() - 30.0f);

    m_spawnCounter++;
    int spawnRate = qMax(5, 30 - m_frameCount / 200 - m_difficulty * 5);
    if (m_spawnCounter > spawnRate)
    {
        m_spawnCounter = 0;
        spawnAsteroid();
    }

    for (int i = m_bullets.size() - 1; i >= 0; i--)
    {
        m_bullets[i].pos.ry() -= m_bullets[i].speed;
        if (m_bullets[i].pos.y() < -20)
        {
            m_bullets.removeAt(i);
        }
    }

    for (int i = m_asteroids.size() - 1; i >= 0; i--)
    {
        m_asteroids[i].pos.ry() += m_asteroids[i].speed;
        if (m_asteroids[i].pos.y() > height() + 50)
        {
            m_asteroids.removeAt(i);
            m_score += 10;
        }
    }

    for (int i = m_particles.size() - 1; i >= 0; i--)
    {
        m_particles[i].pos += m_particles[i].vel;
        m_particles[i].life -= 1.0f;
        m_particles[i].vel.ry() += 0.1f;
        if (m_particles[i].life <= 0)
        {
            m_particles.removeAt(i);
        }
    }

    for (int i = m_scorePopups.size() - 1; i >= 0; i--)
    {
        m_scorePopups[i].pos.ry() -= 1.0f;
        m_scorePopups[i].life--;
        if (m_scorePopups[i].life <= 0)
        {
            m_scorePopups.removeAt(i);
        }
    }

    checkCollisions();

    if (m_frameCount % 60 == 0)
    {
        m_score += 5;
    }

    update();
}

void ShipGame::spawnAsteroid()
{
    Asteroid a;
    a.pos = QPointF(rand() % (width() - 40) + 20.0f, -30.0f);
    a.speed = 3.0f + (float)(rand() % 100) / 100.0f * 6.0f;
    a.speed += m_frameCount * 0.0006f;
    a.speed += m_difficulty * 0.6f;
    a.size = 15.0f + (float)(rand() % 25);
    m_asteroids.append(a);
}

void ShipGame::checkCollisions()
{
    QRectF shipRect(m_shipX - 20, m_shipY - 15, 40, 30);

    for (int i = m_asteroids.size() - 1; i >= 0; i--)
    {
        QRectF ar(m_asteroids[i].pos.x() - m_asteroids[i].size / 2,
                  m_asteroids[i].pos.y() - m_asteroids[i].size / 2,
                  m_asteroids[i].size, m_asteroids[i].size);

        if (shipRect.intersects(ar))
        {
            spawnExplosion(m_asteroids[i].pos, QColor(255, 150, 50), 8);
            m_asteroids.removeAt(i);
            m_lives--;
            m_shakeFrame = 10;
            if (m_lives <= 0)
            {
                m_gameOver = true;
                if (m_score > m_bestScore)
                {
                    m_bestScore = m_score;
                    saveBestScore();
                }
            }
            break;
        }
    }

    for (int bi = m_bullets.size() - 1; bi >= 0; bi--)
    {
        bool bulletHit = false;
        for (int ai = m_asteroids.size() - 1; ai >= 0; ai--)
        {
            float dx = m_bullets[bi].pos.x() - m_asteroids[ai].pos.x();
            float dy = m_bullets[bi].pos.y() - m_asteroids[ai].pos.y();
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < m_asteroids[ai].size / 2 + 4)
            {
                spawnExplosion(m_asteroids[ai].pos, QColor(255, 200, 100), 6);
                spawnScorePopup(m_asteroids[ai].pos, "+50", QColor(255, 220, 100));
                m_asteroids.removeAt(ai);
                m_score += 50;
                bulletHit = true;
                break;
            }
        }
        if (bulletHit)
        {
            m_bullets.removeAt(bi);
        }
    }
}

void ShipGame::renderBackground()
{
    m_bgCache = QPixmap(width(), height());
    m_bgCache.fill(Qt::black);
    QPainter p(&m_bgCache);

    QRadialGradient nebula(width() * 0.3, height() * 0.5, width() * 0.5);
    nebula.setColorAt(0, QColor(15, 5, 35, 50));
    nebula.setColorAt(1, QColor(0, 0, 0, 0));
    p.fillRect(0, 0, width(), height(), nebula);

    p.setPen(Qt::NoPen);
    int seed = (int)m_starSeed;

    for (int i = 0; i < 50; i++)
    {
        int sx = (seed * 1103515245 + 12345 + i * 7) % width();
        int sy = (seed * 1103515245 + 12345 + i * 13) % height();
        int brightness = 140 + ((seed + i * 31) % 116);
        p.setBrush(QColor(brightness, brightness, brightness));
        p.drawRect(sx, sy, 2, 2);
    }

    for (int i = 0; i < 30; i++)
    {
        int sx = (seed * 123456789 + 54321 + i * 11) % width();
        int sy = (seed * 987654321 + 11111 + i * 17) % height();
        int brightness = 80 + ((seed + i * 41) % 100);
        p.setBrush(QColor(brightness, brightness, brightness));
        p.drawRect(sx, sy, 1, 1);
    }
}

void ShipGame::drawBackground(QPainter &p)
{
    if (m_bgCache.isNull())
    {
        renderBackground();
    }
    p.drawPixmap(0, 0, m_bgCache);
}

void ShipGame::drawShip(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);

    if (m_gameOver)
    {
        return;
    }

    if (m_paused && m_frameCount % 20 < 10)
    {
        return;
    }

    p.setPen(Qt::NoPen);
    float cx = m_shipX, cy = m_shipY;

    QPainterPath path;
    path.moveTo(cx, cy - 22);
    path.lineTo(cx + 6, cy - 14);
    path.lineTo(cx + 18, cy + 6);
    path.lineTo(cx + 22, cy + 16);
    path.lineTo(cx + 10, cy + 10);
    path.lineTo(cx + 6, cy + 14);
    path.lineTo(cx, cy + 22);
    path.lineTo(cx - 6, cy + 14);
    path.lineTo(cx - 10, cy + 10);
    path.lineTo(cx - 22, cy + 16);
    path.lineTo(cx - 18, cy + 6);
    path.lineTo(cx - 6, cy - 14);
    path.closeSubpath();

    QLinearGradient bodyGrad(cx, cy - 22, cx, cy + 22);
    bodyGrad.setColorAt(0, QColor(140, 210, 255));
    bodyGrad.setColorAt(0.3, QColor(60, 160, 255));
    bodyGrad.setColorAt(0.6, QColor(30, 100, 220));
    bodyGrad.setColorAt(1, QColor(10, 50, 160));
    p.setBrush(bodyGrad);
    p.drawPath(path);

    QPainterPath wingL;
    wingL.moveTo(cx - 18, cy + 6);
    wingL.lineTo(cx - 28, cy + 22);
    wingL.lineTo(cx - 22, cy + 16);
    wingL.lineTo(cx - 12, cy + 10);
    wingL.closeSubpath();
    QLinearGradient wingLGrad(cx - 18, cy + 6, cx - 28, cy + 22);
    wingLGrad.setColorAt(0, QColor(40, 120, 240));
    wingLGrad.setColorAt(1, QColor(10, 40, 140));
    p.setBrush(wingLGrad);
    p.drawPath(wingL);

    QPainterPath wingR;
    wingR.moveTo(cx + 18, cy + 6);
    wingR.lineTo(cx + 28, cy + 22);
    wingR.lineTo(cx + 22, cy + 16);
    wingR.lineTo(cx + 12, cy + 10);
    wingR.closeSubpath();
    QLinearGradient wingRGrad(cx + 18, cy + 6, cx + 28, cy + 22);
    wingRGrad.setColorAt(0, QColor(40, 120, 240));
    wingRGrad.setColorAt(1, QColor(10, 40, 140));
    p.setBrush(wingRGrad);
    p.drawPath(wingR);

    QRadialGradient cockpitGlow(cx, cy - 8, 10);
    cockpitGlow.setColorAt(0, QColor(255, 255, 200, 200));
    cockpitGlow.setColorAt(0.4, QColor(255, 255, 100, 100));
    cockpitGlow.setColorAt(1, QColor(255, 255, 100, 0));
    p.setBrush(cockpitGlow);
    p.drawEllipse(QPointF(cx, cy - 8), 10, 10);

    p.setBrush(QColor(255, 255, 160));
    p.drawEllipse(QPointF(cx, cy - 8), 5, 5);

    QRadialGradient engineGlow(cx, cy + 20, 10);
    engineGlow.setColorAt(0, QColor(255, 150, 50, 80));
    engineGlow.setColorAt(1, QColor(255, 50, 20, 0));
    p.setBrush(engineGlow);
    p.drawEllipse(QPointF(cx, cy + 20), 10, 10);

    if (m_frameCount % 10 < 5)
    {
        QPainterPath flame;
        flame.moveTo(cx - 3, cy + 20);
        flame.lineTo(cx, cy + 28 + (rand() % 6));
        flame.lineTo(cx + 3, cy + 20);
        flame.closeSubpath();

        QLinearGradient flameGrad(cx, cy + 20, cx, cy + 34);
        flameGrad.setColorAt(0, QColor(255, 180, 60, 200));
        flameGrad.setColorAt(0.5, QColor(255, 100, 30, 140));
        flameGrad.setColorAt(1, QColor(255, 20, 0, 0));
        p.setBrush(flameGrad);
        p.drawPath(flame);
    }
}

void ShipGame::drawBullets(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    for (const auto &b : m_bullets)
    {
        QRadialGradient glow(b.pos.x(), b.pos.y(), 6);
        glow.setColorAt(0, QColor(255, 255, 200, 180));
        glow.setColorAt(0.4, QColor(255, 200, 80, 100));
        glow.setColorAt(1, QColor(255, 100, 30, 0));
        p.setBrush(glow);
        p.drawEllipse(QPointF(b.pos.x(), b.pos.y()), 6, 6);

        QLinearGradient g(b.pos.x(), b.pos.y() - 6, b.pos.x(), b.pos.y() + 6);
        g.setColorAt(0, QColor(255, 255, 180));
        g.setColorAt(1, QColor(255, 150, 50));
        p.setBrush(g);
        p.drawRoundedRect(b.pos.x() - 2, b.pos.y() - 7, 4, 14, 2, 2);
    }
}

void ShipGame::drawParticles(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    for (const auto &pt : m_particles)
    {
        float alpha = pt.life / pt.maxLife;
        QColor c = pt.color;
        c.setAlphaF(alpha);
        p.setBrush(c);
        p.drawEllipse(QPointF(pt.pos.x(), pt.pos.y()), pt.size * alpha, pt.size * alpha);
    }
}

void ShipGame::drawAsteroids(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);

    for (const auto &a : m_asteroids)
    {
        float r = a.size / 2.0f;
        float cx = a.pos.x(), cy = a.pos.y();

        QPainterPath path;
        int n = 8 + (int)(a.size * 0.3f) % 5;
        for (int i = 0; i < n; i++)
        {
            float angle = 2.0f * M_PI * i / n;
            float rr = r * (0.7f + 0.3f * fabsf(sinf((float)((int)(a.pos.x() + i * 37) % 100))));
            float px = cx + cosf(angle) * rr;
            float py = cy + sinf(angle) * rr;
            if (i == 0)
            {
                path.moveTo(px, py);
            }
            else
            {
                path.lineTo(px, py);
            }
        }
        path.closeSubpath();

        QLinearGradient g(cx - r, cy - r, cx + r, cy + r);
        g.setColorAt(0, QColor(160, 140, 110));
        g.setColorAt(0.4, QColor(120, 100, 80));
        g.setColorAt(0.7, QColor(90, 70, 50));
        g.setColorAt(1, QColor(60, 45, 30));
        p.setBrush(g);
        p.setPen(QPen(QColor(80, 60, 40), 1.5));
        p.drawPath(path);

        int craterCount = (int)(a.size * 0.08f);
        p.setPen(Qt::NoPen);
        for (int j = 0; j < craterCount; j++)
        {
            float ca = (float)(((int)(a.pos.x() * 73 + j * 137) % 360)) * M_PI / 180.0f;
            float cd = r * 0.3f + r * 0.5f * ((int)(a.pos.y() + j * 47) % 100) / 100.0f;
            float crx = cx + cosf(ca) * cd;
            float cry = cy + sinf(ca) * cd;
            float csize = 2.0f + (float)(((int)(a.pos.x() + j * 31) % 100)) / 100.0f * (r * 0.25f);

            QRadialGradient craterGrad(crx + 1, cry + 1, csize);
            craterGrad.setColorAt(0, QColor(40, 30, 20, 160));
            craterGrad.setColorAt(0.6, QColor(60, 45, 30, 100));
            craterGrad.setColorAt(1, QColor(120, 100, 80, 0));
            p.setBrush(craterGrad);
            p.drawEllipse(QPointF(crx, cry), csize, csize * 0.7f);
        }
    }
}

void ShipGame::drawUI(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    for (int i = 0; i < m_lives; i++)
    {
        float sx = 30 + i * 30;
        float sy = 28;

        QPainterPath ship;
        ship.moveTo(sx, sy - 8);
        ship.lineTo(sx + 5, sy + 5);
        ship.lineTo(sx + 2, sy + 3);
        ship.lineTo(sx, sy + 8);
        ship.lineTo(sx - 2, sy + 3);
        ship.lineTo(sx - 5, sy + 5);
        ship.closeSubpath();

        QLinearGradient sg(sx, sy - 8, sx, sy + 8);
        sg.setColorAt(0, QColor(140, 210, 255));
        sg.setColorAt(1, QColor(30, 100, 220));
        p.setBrush(sg);
        p.drawPath(ship);

        p.setBrush(QColor(255, 255, 160));
        p.drawEllipse(QPointF(sx, sy - 4), 2, 2);
    }

    QFont f("WenQuanYi Zen Hei", 16, QFont::Bold);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 5, width() - 20, 40), Qt::AlignRight,
               QString("得分: %1").arg(m_score));

    QFont smallFont("WenQuanYi Zen Hei", 10);
    p.setFont(smallFont);
    p.setPen(QColor(180, 180, 180));
    p.drawText(QRect(0, 42, width() - 20, 20), Qt::AlignRight,
               QString("最高分: %1 | 难度: %2").arg(m_bestScore).arg(m_difficulty));

    if (!m_hasSensor && m_fd < 0)
    {
        p.setFont(QFont("WenQuanYi Zen Hei", 10));
        p.setPen(QColor(150, 150, 150));
        p.drawText(QRect(0, height() - 50, width(), 20), Qt::AlignCenter,
                   "未检测到MAG3110，使用 ← → 方向键控制");
        p.drawText(QRect(0, height() - 30, width(), 20), Qt::AlignCenter,
                   "空格键射击 | P 暂停 | ESC 退出");
    }

    if (m_paused)
    {
        p.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 140));
        p.setFont(QFont("WenQuanYi Zen Hei", 28, QFont::Bold));
        p.setPen(QColor(255, 255, 255));
        p.drawText(QRect(0, height() / 2 - 30, width(), 50), Qt::AlignCenter,
                   "暂停中");
        p.setFont(QFont("WenQuanYi Zen Hei", 14));
        p.setPen(QColor(200, 200, 200));
        p.drawText(QRect(0, height() / 2 + 30, width(), 40), Qt::AlignCenter,
                   "按 P 继续");
    }

    if (m_gameOver)
    {
        p.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 160));

        p.setFont(QFont("WenQuanYi Zen Hei", 28, QFont::Bold));
        p.setPen(QColor(255, 100, 100));
        p.drawText(QRect(0, height() / 2 - 80, width(), 50), Qt::AlignCenter,
                   "游戏结束");

        p.setFont(QFont("WenQuanYi Zen Hei", 18));
        p.setPen(Qt::white);
        p.drawText(QRect(0, height() / 2 - 20, width(), 40), Qt::AlignCenter,
                   QString("最终得分: %1").arg(m_score));

        if (m_score >= m_bestScore && m_score > 0)
        {
            p.setPen(QColor(255, 215, 0));
            p.drawText(QRect(0, height() / 2 + 10, width(), 30), Qt::AlignCenter,
                       "新纪录!");
        }

        p.setFont(QFont("WenQuanYi Zen Hei", 14));
        p.setPen(QColor(200, 200, 200));
        p.drawText(QRect(0, height() / 2 + 45, width(), 40), Qt::AlignCenter,
                   "按 R 键重新开始");
    }
}

void ShipGame::drawScorePopups(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing);
    for (const auto &sp : m_scorePopups)
    {
        float alpha = (float)sp.life / 30.0f;
        QColor c = sp.color;
        c.setAlphaF(alpha);
        QFont f("WenQuanYi Zen Hei", 14, QFont::Bold);
        p.setFont(f);
        p.setPen(c);
        p.drawText(QRectF(sp.pos.x() - 30, sp.pos.y() - 15, 60, 30),
                   Qt::AlignCenter, sp.text);
    }
}

void ShipGame::spawnScorePopup(const QPointF &pos, const QString &text, const QColor &color)
{
    ScorePopup sp;
    sp.pos = pos;
    sp.life = 30;
    sp.text = text;
    sp.color = color;
    m_scorePopups.append(sp);
}

void ShipGame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    int shakeX = 0, shakeY = 0;
    if (m_shakeFrame > 0)
    {
        shakeX = (rand() % 5) - 2;
        shakeY = (rand() % 5) - 2;
    }
    p.translate(shakeX, shakeY);

    drawBackground(p);
    drawAsteroids(p);
    drawBullets(p);
    drawParticles(p);
    drawShip(p);
    drawScorePopups(p);
    drawUI(p);
}

void ShipGame::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }

    if (event->key() == Qt::Key_P)
    {
        if (!m_gameOver)
        {
            m_paused = !m_paused;
        }
        return;
    }

    if (event->isAutoRepeat())
    {
        return;
    }

    if (m_gameOver)
    {
        if (event->key() == Qt::Key_R)
        {
            onRestart();
        }
        return;
    }

    if (m_paused)
    {
        return;
    }

    if (m_fd < 0)
    {
        if (event->key() == Qt::Key_Left)
        {
            m_shipX -= 24;
        }
        else if (event->key() == Qt::Key_Right)
        {
            m_shipX += 24;
        }
    }

    if (event->key() == Qt::Key_Space)
    {
        shoot();
    }
}

void ShipGame::keyReleaseEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

void ShipGame::onRestart()
{
    initGame();
    update();
}

void ShipGame::mousePressEvent(QMouseEvent *event)
{
    if (m_gameOver)
    {
        onRestart();
        return;
    }
    m_touchActive = true;
    m_touchX = (float)event->x();
    m_touchShoot = true;
}

void ShipGame::mouseMoveEvent(QMouseEvent *event)
{
    if (m_touchActive)
    {
        m_touchX = (float)event->x();
    }
}

void ShipGame::mouseReleaseEvent(QMouseEvent *)
{
    m_touchActive = false;
    m_touchShoot = false;
}

void ShipGame::resizeEvent(QResizeEvent *)
{
    m_bgCache = QPixmap();
    m_exitBtn->move(width() - 42, 6);
}

void ShipGame::loadBestScore()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    QString path = dir + "/spaceship_score.json";

    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
    {
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        m_bestScore = obj["bestScore"].toInt();
    }
}

void ShipGame::saveBestScore()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    QString path = dir + "/spaceship_score.json";

    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
    {
        QJsonObject obj;
        obj["bestScore"] = m_bestScore;
        f.write(QJsonDocument(obj).toJson());
    }
}