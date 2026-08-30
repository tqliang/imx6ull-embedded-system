#include "networksettingswidget.h"
#include "settingsmanager.h"
#include "networkkeyboard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QMessageBox>
#include <QLineEdit>

NetworkSettingsWidget::NetworkSettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("网络设置");

    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    QFont titleFont("WenQuanYi Zen Hei", 18, QFont::Bold);
    QFont labelFont("WenQuanYi Zen Hei", 13);
    QFont inputFont("WenQuanYi Zen Hei", 12);
    QFont btnFont("WenQuanYi Zen Hei", 13, QFont::Bold);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel("网络参数设置", this);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: white; padding: 6px;");
    mainLayout->addWidget(titleLabel);

    QGroupBox *mqttGroup = new QGroupBox("MQTT 配置", this);
    mqttGroup->setFont(labelFont);
    mqttGroup->setStyleSheet(
        "QGroupBox { color: #64B5F6; border: 1px solid #333; border-radius: 6px; "
        "margin-top: 12px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }");

    QFormLayout *mqttLayout = new QFormLayout;
    mqttLayout->setSpacing(10);
    mqttLayout->setContentsMargins(12, 8, 12, 8);

    m_mqttHostEdit = new QLineEdit(this);
    m_mqttHostEdit->setFont(inputFont);
    m_mqttHostEdit->setMinimumHeight(38);
    m_mqttHostEdit->setPlaceholderText("例如: 192.168.1.100");
    m_mqttHostEdit->setStyleSheet(
        "QLineEdit { background: #1e1e2e; color: white; border: 1px solid #444; "
        "border-radius: 6px; padding: 8px; }"
        "QLineEdit:focus { border: 1px solid #64B5F6; }");

    QLabel *hostLabel = new QLabel("服务器地址:", this);
    hostLabel->setFont(labelFont);
    hostLabel->setStyleSheet("color: #ccc;");
    mqttLayout->addRow(hostLabel, m_mqttHostEdit);

    m_mqttPortSpin = new QSpinBox(this);
    m_mqttPortSpin->setFont(inputFont);
    m_mqttPortSpin->setMinimumHeight(38);
    m_mqttPortSpin->setRange(1, 65535);
    m_mqttPortSpin->setStyleSheet(
        "QSpinBox { background: #1e1e2e; color: white; border: 1px solid #444; "
        "border-radius: 6px; padding: 8px; }"
        "QSpinBox:focus { border: 1px solid #64B5F6; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 0px; }");

    QLabel *portLabel = new QLabel("端口:", this);
    portLabel->setFont(labelFont);
    portLabel->setStyleSheet("color: #ccc;");
    mqttLayout->addRow(portLabel, m_mqttPortSpin);

    mqttGroup->setLayout(mqttLayout);
    mainLayout->addWidget(mqttGroup);

    QGroupBox *deviceGroup = new QGroupBox("设备信息", this);
    deviceGroup->setFont(labelFont);
    deviceGroup->setStyleSheet(
        "QGroupBox { color: #81C784; border: 1px solid #333; border-radius: 6px; "
        "margin-top: 12px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }");

    QFormLayout *deviceLayout = new QFormLayout;
    deviceLayout->setSpacing(10);
    deviceLayout->setContentsMargins(12, 8, 12, 8);

    m_deviceNameEdit = new QLineEdit(this);
    m_deviceNameEdit->setFont(inputFont);
    m_deviceNameEdit->setMinimumHeight(38);
    m_deviceNameEdit->setPlaceholderText("例如: 客厅设备");
    m_deviceNameEdit->setStyleSheet(
        "QLineEdit { background: #1e1e2e; color: white; border: 1px solid #444; "
        "border-radius: 6px; padding: 8px; }"
        "QLineEdit:focus { border: 1px solid #81C784; }");

    QLabel *devLabel = new QLabel("设备名称:", this);
    devLabel->setFont(labelFont);
    devLabel->setStyleSheet("color: #ccc;");
    deviceLayout->addRow(devLabel, m_deviceNameEdit);

    m_wifiCheck = new QCheckBox("启用 WiFi", this);
    m_wifiCheck->setFont(labelFont);
    m_wifiCheck->setStyleSheet(
        "QCheckBox { color: #ccc; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { background: #1e1e2e; border: 1px solid #444; border-radius: 4px; }"
        "QCheckBox::indicator:checked { background: #81C784; border: 1px solid #81C784; border-radius: 4px; }");
    deviceLayout->addRow(m_wifiCheck);

    deviceGroup->setLayout(deviceLayout);
    mainLayout->addWidget(deviceGroup);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setFont(QFont("WenQuanYi Zen Hei", 11));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #4CAF50; padding: 4px;");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(16);

    QPushButton *saveBtn = new QPushButton("保存", this);
    saveBtn->setMinimumSize(110, 42);
    saveBtn->setFont(btnFont);
    saveBtn->setStyleSheet(
        "QPushButton { background: #4CAF50; color: white; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #388E3C; }");
    connect(saveBtn, &QPushButton::clicked, this, &NetworkSettingsWidget::onSave);
    btnRow->addWidget(saveBtn);

    QPushButton *exitBtn = new QPushButton("退出", this);
    exitBtn->setMinimumSize(110, 42);
    exitBtn->setFont(btnFont);
    exitBtn->setStyleSheet(
        "QPushButton { background: white; color: #333; border: none; border-radius: 8px; }"
        "QPushButton:pressed { background: #ccc; }");
    connect(exitBtn, &QPushButton::clicked, this, &NetworkSettingsWidget::onExit);
    btnRow->addWidget(exitBtn);

    mainLayout->addLayout(btnRow);

    setStyleSheet("background: #12121e;");

    m_keyboard = new NetworkKeyboard(this);
    connect(m_keyboard, &NetworkKeyboard::keyPressed, this, &NetworkSettingsWidget::onKeyPressed);
    connect(qApp, &QApplication::focusChanged, this, &NetworkSettingsWidget::onFocusChanged);

    SettingsManager *s = SettingsManager::instance();
    m_mqttHostEdit->setText(s->mqttHost());
    m_mqttPortSpin->setValue(s->mqttPort());
    m_deviceNameEdit->setText(s->deviceName());
    m_wifiCheck->setChecked(s->wifiEnabled());
}

NetworkSettingsWidget::~NetworkSettingsWidget()
{
}

void NetworkSettingsWidget::resizeEvent(QResizeEvent *)
{
    m_keyboard->setFixedWidth(width());
}

void NetworkSettingsWidget::onFocusChanged(QWidget *, QWidget *now)
{
    if (now == m_mqttHostEdit || now == m_deviceNameEdit)
    {
        m_keyboard->popup();
    }
    else
    {
        m_keyboard->dismiss();
    }
}

void NetworkSettingsWidget::onSave()
{
    m_keyboard->dismiss();
    m_mqttHostEdit->clearFocus();
    m_deviceNameEdit->clearFocus();

    SettingsManager *s = SettingsManager::instance();
    s->setMqttHost(m_mqttHostEdit->text());
    s->setMqttPort(m_mqttPortSpin->value());
    s->setDeviceName(m_deviceNameEdit->text());
    s->setWifiEnabled(m_wifiCheck->isChecked());

    m_statusLabel->setText("设置已保存，重启后生效");
    m_statusLabel->setStyleSheet("color: #4CAF50; padding: 4px;");
}

void NetworkSettingsWidget::onKeyPressed(const QString &text)
{
    QWidget *w = QApplication::focusWidget();
    QLineEdit *edit = qobject_cast<QLineEdit *>(w);
    if (!edit)
    {
        return;
    }

    if (text == "\b")
    {
        edit->backspace();
    }
    else if (text == "\n")
    {
        edit->clearFocus();
        m_keyboard->dismiss();
    }
    else
    {
        edit->insert(text);
    }
}

void NetworkSettingsWidget::onExit()
{
    qApp->quit();
}