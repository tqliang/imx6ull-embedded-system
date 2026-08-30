#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    static SettingsManager *instance();

    void load();
    void save();

    QString mqttHost() const;
    void setMqttHost(const QString &host);
    int mqttPort() const;
    void setMqttPort(int port);

    QString deviceName() const;
    void setDeviceName(const QString &name);
    int brightness() const;
    void setBrightness(int level);

    bool wifiEnabled() const;
    void setWifiEnabled(bool enabled);

private:
    explicit SettingsManager(QObject *parent = nullptr);
    QString configPath() const;

    QJsonObject m_data;
    static SettingsManager *s_instance;
};

#endif