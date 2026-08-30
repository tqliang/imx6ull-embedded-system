#include "backlightpanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QFont>
#include <cmath>

BacklightPanel::BacklightPanel(QWidget *parent)
    : QWidget(parent)
    , m_slideOffset(-100)
    , m_panelHeight(100)
    , m_visible(false)
{
    setFixedHeight(m_panelHeight);

    m_anim = new QPropertyAnimation(this, "slideOffset", this);
    m_anim->setDuration(250);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(3000);
    connect(m_hideTimer, &QTimer::timeout, this, &BacklightPanel::hidePanel);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 7);
    m_slider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  border: none; height: 6px;"
        "  background: rgba(255,255,255,40);"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #FFD54F;"
        "  width: 28px; height: 28px;"
        "  margin: -11px 0;"
        "  border-radius: 14px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #FFD54F;"
        "  border-radius: 3px;"
        "}");

    m_label = new QLabel(this);
    m_label->setFont(QFont("WenQuanYi Zen Hei", 14, QFont::Bold));
    m_label->setStyleSheet("color: #FFD54F; background: transparent;");
    m_label->setFixedWidth(40);
    m_label->setAlignment(Qt::AlignCenter);

    connect(m_slider, &QSlider::valueChanged, this, [this](int val) {
        m_label->setText(QString::number(val));
        m_hideTimer->start();
        emit brightnessChanged(val);
    });

    QString btnStyle =
        "QPushButton { background: #2a2a3e; color: #FFD54F; border: 1px solid #3a3a4e;"
        "  border-radius: 3px; font-size: 16px; font-weight: bold; }"
        "QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }";

    m_minusBtn = new QPushButton("-", this);
    m_minusBtn->setFixedSize(30, 30);
    m_minusBtn->setFont(QFont("WenQuanYi Zen Hei", 16, QFont::Bold));
    m_minusBtn->setStyleSheet(btnStyle);

    m_plusBtn = new QPushButton("+", this);
    m_plusBtn->setFixedSize(30, 30);
    m_plusBtn->setFont(QFont("WenQuanYi Zen Hei", 16, QFont::Bold));
    m_plusBtn->setStyleSheet(btnStyle);

    connect(m_minusBtn, &QPushButton::clicked, this, [this]() {
        int v = m_slider->value() - 1;
        if (v >= 0)
        {
            m_slider->setValue(v);
        }
    });
    connect(m_plusBtn, &QPushButton::clicked, this, [this]() {
        int v = m_slider->value() + 1;
        if (v <= 7)
        {
            m_slider->setValue(v);
        }
    });

    QHBoxLayout *ctrlLayout = new QHBoxLayout;
    ctrlLayout->setContentsMargins(60, 30, 10, 0);
    ctrlLayout->setSpacing(8);
    ctrlLayout->addWidget(m_minusBtn);
    ctrlLayout->addWidget(m_slider, 1);
    ctrlLayout->addWidget(m_plusBtn);
    ctrlLayout->addWidget(m_label);
    setLayout(ctrlLayout);

    hide();
}

void BacklightPanel::setSlideOffset(int offset)
{
    m_slideOffset = offset;
    QWidget *p = parentWidget();
    if (p)
    {
        move(0, m_slideOffset);
        int w = p->width();
        setFixedWidth(w > 0 ? w : 480);
    }
}

void BacklightPanel::showPanel()
{
    if (m_visible)
    {
        return;
    }
    m_visible = true;
    show();
    raise();

    QWidget *p = parentWidget();
    if (p)
    {
        setFixedWidth(p->width());
    }

    m_anim->stop();
    m_anim->setStartValue(-m_panelHeight);
    m_anim->setEndValue(0);
    m_anim->start();

    m_hideTimer->start();
}

void BacklightPanel::hidePanel()
{
    if (!m_visible)
    {
        return;
    }
    m_visible = false;

    m_anim->stop();
    m_anim->setStartValue(m_slideOffset);
    m_anim->setEndValue(-m_panelHeight);
    m_anim->start();

    QTimer::singleShot(300, this, &QWidget::hide);
}

void BacklightPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 45, 230));
    p.drawRoundedRect(rect().adjusted(0, 0, 0, 8), 0, 0);

    QRect bottomPart(0, height() - 8, width(), 8);
    p.drawRect(bottomPart);

    int iconX = 22;
    int iconY = 22;
    int iconR = 14;

    p.setBrush(QColor("#FFD54F"));
    p.drawEllipse(QPoint(iconX + iconR, iconY + iconR), iconR, iconR);

    p.setPen(QPen(QColor("#FFD54F"), 2));
    for (int i = 0; i < 8; i++)
    {
        double angle = (i * 45 - 90) * M_PI / 180.0;
        int cx = iconX + iconR;
        int cy = iconY + iconR;
        int inner = iconR - 4;
        int outer = iconR + 2;
        int x1 = cx + (int)(inner * cos(angle));
        int y1 = cy + (int)(inner * sin(angle));
        int x2 = cx + (int)(outer * cos(angle));
        int y2 = cy + (int)(outer * sin(angle));
        p.drawLine(x1, y1, x2, y2);
    }
}