#ifndef NETWORKSETTINGSWIDGET_H
#define NETWORKSETTINGSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

class NetworkKeyboard;

class NetworkSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NetworkSettingsWidget(QWidget *parent = nullptr);
    ~NetworkSettingsWidget();

protected:
    void resizeEvent(QResizeEvent *) override;

private slots:
    void onSave();
    void onExit();
    void onKeyPressed(const QString &text);
    void onFocusChanged(QWidget *old, QWidget *now);

private:
    QLineEdit       *m_mqttHostEdit;
    QSpinBox        *m_mqttPortSpin;
    QLineEdit       *m_deviceNameEdit;
    QCheckBox       *m_wifiCheck;
    QLabel          *m_statusLabel;
    NetworkKeyboard *m_keyboard;
};

#endif