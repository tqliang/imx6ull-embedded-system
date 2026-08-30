#include "networkkeyboard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFont>
#include <QPainter>
#include <QApplication>

NetworkKeyboard::NetworkKeyboard(QWidget *parent)
    : QWidget(parent), m_shifted(false)
{
    setFixedHeight(220);
    hide();

    m_keyContainer = new QWidget(this);
    m_keyContainer->setStyleSheet("background: #1a1a2e;");

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_keyContainer);

    buildLayout();

    m_anim = new QPropertyAnimation(this, "pos", this);
    m_anim->setDuration(200);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void NetworkKeyboard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(QPen(QColor("#2a2a4e"), 2));
    p.drawLine(0, 0, width(), 0);
}

void NetworkKeyboard::popup()
{
    if (isVisible()) return;
    QWidget *p = parentWidget();
    if (!p) return;
    int targetY = p->height() - height();
    m_anim->setStartValue(QPoint(0, p->height()));
    m_anim->setEndValue(QPoint(0, targetY));
    move(0, p->height());
    show();
    raise();
    m_anim->start();
}

void NetworkKeyboard::dismiss()
{
    if (!isVisible()) return;
    QWidget *p = parentWidget();
    if (!p) { hide(); return; }
    m_anim->setStartValue(pos());
    m_anim->setEndValue(QPoint(0, p->height()));
    connect(m_anim, &QPropertyAnimation::finished, this, &QWidget::hide, Qt::UniqueConnection);
    m_anim->start();
}

QPushButton *NetworkKeyboard::createKey(const QString &text, int h, bool emitText)
{
    QPushButton *btn = new QPushButton(text, m_keyContainer);
    btn->setFixedHeight(h);
    btn->setFont(QFont("WenQuanYi Zen Hei", 12, QFont::Bold));
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setStyleSheet(
        "QPushButton { background: #2a2a3e; color: #ddd; border: 1px solid #3a3a4e;"
        "  border-radius: 6px; }"
        "QPushButton:pressed { background: #4a4a6e; color: #fff; }");
    if (emitText)
    {
        connect(btn, &QPushButton::clicked, this, [this, text]() {
            emit keyPressed(text);
        });
    }
    return btn;
}

void NetworkKeyboard::buildLayout()
{
    QGridLayout *grid = new QGridLayout(m_keyContainer);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    for (int c = 0; c < 10; c++)
        grid->setColumnStretch(c, 1);

    const int kKeyH = 40;
    const int kSpecH = 44;

    int row = 0;

    QStringList row1 = {"1","2","3","4","5","6","7","8","9","0"};
    for (int c = 0; c < row1.size(); c++)
        grid->addWidget(createKey(row1[c], kKeyH), row, c);
    row++;

    QStringList row2 = {"q","w","e","r","t","y","u","i","o","p"};
    for (int c = 0; c < row2.size(); c++)
    {
        QPushButton *btn = createKey(row2[c], kKeyH);
        grid->addWidget(btn, row, c);
        m_letterKeys.append(btn);
    }
    row++;

    QStringList row3 = {"a","s","d","f","g","h","j","k","l"};
    for (int c = 0; c < row3.size(); c++)
    {
        QPushButton *btn = createKey(row3[c], kKeyH);
        grid->addWidget(btn, row, c + 1);
        m_letterKeys.append(btn);
    }
    row++;

    QPushButton *shiftBtn = createKey("Shift", kSpecH, false);
    shiftBtn->setStyleSheet(
        "QPushButton { background: #3a3a50; color: #FFD54F; border: 1px solid #4a4a60;"
        "  border-radius: 6px; font-size: 11px; }"
        "QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }");
    connect(shiftBtn, &QPushButton::clicked, this, [this, shiftBtn]() {
        m_shifted = !m_shifted;
        for (QPushButton *btn : m_letterKeys)
        {
            QString t = btn->text();
            btn->setText(m_shifted ? t.toUpper() : t.toLower());
        }
        shiftBtn->setStyleSheet(m_shifted
            ? "QPushButton { background: #FFD54F; color: #1a1a2e; border: 1px solid #FFD54F;"
              "  border-radius: 6px; font-size: 11px; }"
            : "QPushButton { background: #3a3a50; color: #FFD54F; border: 1px solid #4a4a60;"
              "  border-radius: 6px; font-size: 11px; }"
              "QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }");
    });
    grid->addWidget(shiftBtn, row, 0, 1, 2);

    QStringList row4 = {"z","x","c","v","b","n","m"};
    for (int c = 0; c < row4.size(); c++)
    {
        QPushButton *btn = createKey(row4[c], kKeyH);
        grid->addWidget(btn, row, c + 2);
        m_letterKeys.append(btn);
    }

    QPushButton *bsBtn = createKey("⌫", kSpecH, false);
    bsBtn->setStyleSheet(
        "QPushButton { background: #3a3a50; color: #ff6666; border: 1px solid #4a4a60;"
        "  border-radius: 6px; }"
        "QPushButton:pressed { background: #ff4444; color: #fff; }");
    connect(bsBtn, &QPushButton::clicked, this, [this]() {
        emit keyPressed("\b");
    });
    grid->addWidget(bsBtn, row, 9);
    row++;

    QPushButton *numBtn = createKey("123", kSpecH, false);
    numBtn->setStyleSheet(
        "QPushButton { background: #3a3a50; color: #aaa; border: 1px solid #4a4a60;"
        "  border-radius: 6px; font-size: 11px; }"
        "QPushButton:pressed { background: #5a5a70; }");
    grid->addWidget(numBtn, row, 0);

    QPushButton *dashBtn = createKey("-", kSpecH);
    grid->addWidget(dashBtn, row, 1);

    QPushButton *slashBtn = createKey("/", kSpecH);
    grid->addWidget(slashBtn, row, 2);

    QPushButton *spaceBtn = createKey("Space", kSpecH, false);
    connect(spaceBtn, &QPushButton::clicked, this, [this]() {
        emit keyPressed(" ");
    });
    grid->addWidget(spaceBtn, row, 3, 1, 3);

    QPushButton *dotBtn = createKey(".", kSpecH);
    grid->addWidget(dotBtn, row, 6);

    QPushButton *colonBtn = createKey(":", kSpecH);
    grid->addWidget(colonBtn, row, 7);

    QPushButton *atBtn = createKey("@", kSpecH);
    grid->addWidget(atBtn, row, 8);

    QPushButton *enterBtn = createKey("Enter", kSpecH, false);
    enterBtn->setStyleSheet(
        "QPushButton { background: #2a5a2e; color: #00e676; border: 1px solid #3a7a3e;"
        "  border-radius: 6px; font-size: 11px; }"
        "QPushButton:pressed { background: #00e676; color: #1a1a2e; }");
    connect(enterBtn, &QPushButton::clicked, this, [this]() {
        emit keyPressed("\n");
    });
    grid->addWidget(enterBtn, row, 9);
}