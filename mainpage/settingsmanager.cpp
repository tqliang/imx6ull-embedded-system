#include "settingsmanager.h"
#include <QJsonDocument>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

SettingsManager *SettingsManager::s_instance = nullptr;

SettingsManager *SettingsManager::instance()
{
    if (!s_instance) 
    {
        s_instance = new SettingsManager;
        s_instance->load();
    }
    return s_instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
}

QString SettingsManager::configPath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/settings.json";
}

void SettingsManager::load()
{
    bool existed = false;
    QFile f(configPath());
    if (f.open(QIODevice::ReadOnly)) 
    {
        m_data = QJsonDocument::fromJson(f.readAll()).object();
        existed = true;
        qDebug() << "SETTINGS: 已加载配置" << configPath();
    }

    if (!m_data.contains("mqttHost"))    m_data["mqttHost"] = "172.24.184.154";
    if (!m_data.contains("mqttPort"))    m_data["mqttPort"] = 1883;
    if (!m_data.contains("deviceName"))  m_data["deviceName"] = "IMX6ULL-001";
    if (!m_data.contains("brightness"))  m_data["brightness"] = 7;
    if (!m_data.contains("wifiEnabled")) m_data["wifiEnabled"] = false;

    if (!existed) 
    {
        save();
    }
}

void SettingsManager::save()
{
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly)) 
    {
        f.write(QJsonDocument(m_data).toJson());
        qDebug() << "SETTINGS: 配置已保存" << configPath();
    }
}

QString SettingsManager::mqttHost() const     { return m_data["mqttHost"].toString(); }
void SettingsManager::setMqttHost(const QString &h) { m_data["mqttHost"] = h; save(); }
int SettingsManager::mqttPort() const          { return m_data["mqttPort"].toInt(); }
void SettingsManager::setMqttPort(int p)       { m_data["mqttPort"] = p; save(); }
QString SettingsManager::deviceName() const    { return m_data["deviceName"].toString(); }
void SettingsManager::setDeviceName(const QString &n) { m_data["deviceName"] = n; save(); }
int SettingsManager::brightness() const        { return m_data["brightness"].toInt(); }
void SettingsManager::setBrightness(int l)     { m_data["brightness"] = l; save(); }
bool SettingsManager::wifiEnabled() const      { return m_data["wifiEnabled"].toBool(); }
void SettingsManager::setWifiEnabled(bool e)   { m_data["wifiEnabled"] = e; save(); }