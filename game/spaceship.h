#ifndef SPACESHIP_H
#define SPACESHIP_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointF>
#include <QList>
#include <QMouseEvent>
#include <QPushButton>

struct Asteroid {
    QPointF pos;
    float   speed;
    float   size;
};

struct Bullet {
    QPointF pos;
    float   speed;
};

struct Particle {
    QPointF pos;
    QPointF vel;
    float   life;
    float   maxLife;
    QColor  color;
    float   size;
};

struct ScorePopup {
    QPointF pos;
    int     life;
    QString text;
    QColor  color;
};

class ShipGame : public QWidget
{
    Q_OBJECT
public:
    explicit ShipGame(QWidget *parent = nullptr);
    ~ShipGame();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void gameLoop();
    void onRestart();

private:
    void initGame();
    bool openMagnetometer();
    void readMagnetometer();
    void spawnAsteroid();
    void checkCollisions();
    void renderBackground();
    void drawBackground(QPainter &p);
    void drawShip(QPainter &p);
    void drawAsteroids(QPainter &p);
    void drawBullets(QPainter &p);
    void drawParticles(QPainter &p);
    void drawUI(QPainter &p);
    void drawScorePopups(QPainter &p);
    void spawnExplosion(const QPointF &pos, const QColor &color, int count);
    void spawnScorePopup(const QPointF &pos, const QString &text, const QColor &color);
    void loadBestScore();
    void saveBestScore();
    void shoot();

    QTimer       *m_timer;
    QElapsedTimer m_elapsed;
    QPixmap       m_bgCache;

    int     m_fd;
    float   m_heading;
    float   m_shipX;
    float   m_shipY;
    int     m_lives;
    int     m_score;
    int     m_bestScore;
    int     m_difficulty;
    bool    m_gameOver;
    bool    m_paused;
    bool    m_hasSensor;
    int     m_frameCount;
    float   m_starSeed;
    int     m_shootCooldown;

    QList<Asteroid>   m_asteroids;
    QList<Bullet>     m_bullets;
    QList<Particle>   m_particles;
    QList<ScorePopup> m_scorePopups;
    int               m_spawnCounter;
    int               m_shakeFrame;

    bool    m_touchActive;
    bool    m_touchShoot;
    float   m_touchX;

    QPushButton *m_exitBtn;
};

#endif