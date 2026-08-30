#include "softkeyboard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFont>
#include <QApplication>

SoftKeyboard::SoftKeyboard(QWidget *parent)
    : QWidget(parent), m_shifted(false)
{
    setFixedHeight(200);

    m_keyContainer = new QWidget(this);
    m_keyContainer->setStyleSheet("background: #1a1a2e;");

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_keyContainer);

    buildLayout();
}

QPushButton *SoftKeyboard::createKey(const QString &text, int w, int h, bool emitText)
{
    QPushButton *btn = new QPushButton(text, m_keyContainer);
    if (w > 0)
        btn->setFixedWidth(w);
    btn->setFixedHeight(h);
    btn->setFont(QFont("WenQuanYi Zen Hei", 12, QFont::Bold));
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setStyleSheet(
        "QPushButton { background: #2a2a3e; color: #ddd; border: 1px solid #3a3a4e;"
        "  border-radius: 5px; }"
        "QPushButton:pressed { background: #4a4a6e; color: #fff; }");
    if (emitText) 
    {
        connect(btn, &QPushButton::clicked, this, [this, text]() {
            emit keyPressed(text);
        });
    }
    return btn;
}

void SoftKeyboard::buildLayout()
{
    QGridLayout *grid = new QGridLayout(m_keyContainer);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    for (int c = 0; c < 10; c++)
        grid->setColumnStretch(c, 1);

    const int kKeyH = 38;
    const int kSpecH = 42;

    int row = 0;

    QStringList row1 = {"1","2","3","4","5","6","7","8","9","0"};
    for (int c = 0; c < row1.size(); c++)
        grid->addWidget(createKey(row1[c], 0, kKeyH), row, c);
    row++;

    QStringList row2 = {"q","w","e","r","t","y","u","i","o","p"};
    for (int c = 0; c < row2.size(); c++) 
    {
        QPushButton *btn = createKey(row2[c], 0, kKeyH);
        grid->addWidget(btn, row, c);
        m_letterKeys.append(btn);
    }
    row++;

    QStringList row3 = {"a","s","d","f","g","h","j","k","l"};
    for (int c = 0; c < row3.size(); c++) 
    {
        QPushButton *btn = createKey(row3[c], 0, kKeyH);
        grid->addWidget(btn, row, c + 1);
        m_letterKeys.append(btn);
    }
    row++;

    QPushButton *shiftBtn = createKey("Shift", 0, kSpecH, false);
    shiftBtn->setStyleSheet(
        "QPushButton { background: #3a3a50; color: #FFD54F; border: 1px solid #4a4a60;"
        "  border-radius: 5px; font-size: 11px; }"
        "QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }");
    connect(shiftBtn, &QPushButton::clicked, this, [this, shiftBtn]() {
        m_shifted = !m_shifted;
        for (QPushButton *btn : m_letterKeys) 
        {
            QString t = btn->text();
            if (m_shifted)
                btn->setText(t.toUpper());
            else
                btn->setText(t.toLower());
        }
        shiftBtn->setStyleSheet(m_shifted
            ? "QPushButton { background: #FFD54F; color: #1a1a2e; border: 1px solid #FFD54F;"
              "  border-radius: 5px; font-size: 11px; }"
            : "QPushButton { background: #3a3a50; color: #FFD54F; border: 1px solid #4a4a60;"
              "  border-radius: 5px; font-size: 11px; }"
              "QPushButton:pressed { background: #FFD54F; color: #1a1a2e; }");
    });
    grid->addWidget(shiftBtn, row, 0, 1, 2);

    QStringList row4 = {"z","x","c","v","b","n","m"};
    for (int c = 0; c < row4.size(); c++) 
    {
        QPushButton *btn = createKey(row4[c], 0, kKeyH);
        grid->addWidget(btn, row, c + 2);
        m_letterKeys.append(btn);
    }

    QPushButton *bsBtn = createKey("⌫", 0, kSpecH, false);
    bsBtn->setStyleSheet(
        "QPushButton { background: #3a3a50; color: #ff6666; border: 1px solid #4a4a60;"
        "  border-radius: 5px; }"
        "QPushButton:pressed { background: #ff4444; color: #fff; }");
    connect(bsBtn, &QPushButton::clicked, this, [this]() {
        emit keyPressed("\b");
    });
    grid->addWidget(bsBtn, row, 9);
    row++;

    QPushButton *numBtn = createKey("123", 0, kSpecH, false);
    numBtn->setStyleSheet(
        "QPushButton { background: #3a3a50; color: #aaa; border: 1px solid #4a4a60;"
        "  border-radius: 5px; font-size: 11px; }"
        "QPushButton:pressed { background: #5a5a70; }");
    grid->addWidget(numBtn, row, 0);

    QPushButton *dashBtn = createKey("-", 0, kSpecH);
    grid->addWidget(dashBtn, row, 1);

    QPushButton *slashBtn = createKey("/", 0, kSpecH);
    grid->addWidget(slashBtn, row, 2);

    QPushButton *spaceBtn = createKey("Space", 0, kSpecH, false);
    connect(spaceBtn, &QPushButton::clicked, this, [this]() {
        emit keyPressed(" ");
    });
    grid->addWidget(spaceBtn, row, 3, 1, 3);

    QPushButton *dotBtn = createKey(".", 0, kSpecH);
    grid->addWidget(dotBtn, row, 6);

    QPushButton *colonBtn = createKey(":", 0, kSpecH);
    grid->addWidget(colonBtn, row, 7);

    QPushButton *atBtn = createKey("@", 0, kSpecH);
    grid->addWidget(atBtn, row, 8);

    QPushButton *enterBtn = createKey("Enter", 0, kSpecH, false);
    enterBtn->setStyleSheet(
        "QPushButton { background: #2a5a2e; color: #00e676; border: 1px solid #3a7a3e;"
        "  border-radius: 5px; font-size: 11px; }"
        "QPushButton:pressed { background: #00e676; color: #1a1a2e; }");
    connect(enterBtn, &QPushButton::clicked, this, [this]() {
        emit keyPressed("\n");
    });
    grid->addWidget(enterBtn, row, 9);
}