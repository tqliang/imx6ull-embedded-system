#ifndef BACKLIGHTPANEL_H
#define BACKLIGHTPANEL_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QPropertyAnimation>
#include <QTimer>

class BacklightPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int slideOffset READ slideOffset WRITE setSlideOffset)
public:
    explicit BacklightPanel(QWidget *parent = nullptr);

    int slideOffset() const { return m_slideOffset; }
    void setSlideOffset(int offset);

    void showPanel();
    void hidePanel();
    void setValue(int level) { m_slider->setValue(level); }

signals:
    void brightnessChanged(int level);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QSlider            *m_slider;
    QLabel             *m_label;
    QPushButton        *m_minusBtn;
    QPushButton        *m_plusBtn;
    QPropertyAnimation *m_anim;
    QTimer             *m_hideTimer;
    int                 m_slideOffset;
    int                 m_panelHeight;
    bool                m_visible;
};

#endif