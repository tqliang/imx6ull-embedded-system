#ifndef SOFTKEYBOARD_H
#define SOFTKEYBOARD_H

#include <QWidget>
#include <QPushButton>

class SoftKeyboard : public QWidget
{
    Q_OBJECT
public:
    explicit SoftKeyboard(QWidget *parent = nullptr);

signals:
    void keyPressed(const QString &text);

private:
    QPushButton *createKey(const QString &text, int w = 40, int h = 36, bool emitText = true);
    void buildLayout();

    QWidget            *m_keyContainer;
    bool                m_shifted;
    QList<QPushButton *> m_letterKeys;
};

#endif