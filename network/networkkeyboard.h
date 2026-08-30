#ifndef NETWORKKEYBOARD_H
#define NETWORKKEYBOARD_H

#include <QWidget>
#include <QPushButton>
#include <QPropertyAnimation>

class NetworkKeyboard : public QWidget
{
    Q_OBJECT
public:
    explicit NetworkKeyboard(QWidget *parent = nullptr);

    void popup();
    void dismiss();

signals:
    void keyPressed(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPushButton *createKey(const QString &text, int h, bool emitText = true);
    void buildLayout();

    QWidget            *m_keyContainer;
    bool                m_shifted;
    QList<QPushButton *> m_letterKeys;
    QPropertyAnimation  *m_anim;
};

#endif