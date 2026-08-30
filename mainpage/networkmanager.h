#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QJsonObject>
#include <QMutex>
#include <linux/videodev2.h>

struct V4L2Buffer {
    void   *start;
    size_t  length;
};

class NetworkWorker : public QObject
{
    Q_OBJECT
public:
    explicit NetworkWorker(QObject *parent = nullptr);

public slots:
    void connectToMqtt(const QString &host, int port);
    void publishStatus(const QJsonObject &status);
    void startHttpServer(int port);
    void stop();
    void startStream();
    void stopStream();
    bool isStreaming() const { return m_streaming; }
    void setAppRunning(bool running) { m_appRunning = running; }

signals:
    void connected();
    void disconnected();
    void remoteCommand(const QString &cmd, const QJsonObject &params);
    void statusMessage(const QString &msg);

private slots:
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttReadyRead();
    void onNewHttpConnection();
    void onHttpReadyRead();
    void onReconnectTimer();
    void onStreamTimer();

private:
    void sendMqttConnect();
    void sendMqttPublish(const QString &topic, const QByteArray &payload);
    void sendMqttSubscribe(const QString &topic);
    void handleHttpRequest(QTcpSocket *client, const QByteArray &data);
    QByteArray buildHttpResponse(const QString &body, const QString &contentType = "text/html");
    QByteArray encodeRemainingLength(int length);
    QByteArray buildMqttPacket(unsigned char type, const QByteArray &payload);

    bool openCamera();
    void closeCamera();
    bool grabFrame(QByteArray &jpegData);

    QTcpSocket         *m_mqttSocket;
    QTcpServer         *m_httpServer;
    QList<QTcpSocket*>  m_httpClients;
    QList<QTcpSocket*>  m_streamClients;
    QTimer             *m_reconnectTimer;
    QTimer             *m_pingTimer;
    QTimer             *m_streamTimer;
    QString             m_host;
    int                 m_port;
    bool                m_connected;
    QString             m_subscribeTopic;

    int                 m_cameraFd;
    V4L2Buffer         *m_cameraBuffers;
    int                 m_cameraBufCount;
    QMutex              m_cameraMutex;
    unsigned int        m_pixelFormat;
    int                 m_streamWidth;
    int                 m_streamHeight;
    bool                m_streaming;
    bool                m_appRunning;
};

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    static NetworkManager *instance();

    void start();
    void stop();

    void publishStatus(const QJsonObject &status);
    void startStream();
    void stopStream();
    bool isStreaming() const;
    void setAppRunning(bool running);
    int  httpPort() const { return 8080; }

signals:
    void connected();
    void disconnected();
    void remoteCommand(const QString &cmd, const QJsonObject &params);
    void statusMessage(const QString &msg);

private:
    explicit NetworkManager(QObject *parent = nullptr);
    QThread         m_workerThread;
    NetworkWorker  *m_worker;
    static NetworkManager *s_instance;
};

#endif