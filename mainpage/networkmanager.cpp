#include "networkmanager.h"
#include "settingsmanager.h"
#include <QJsonDocument>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QImage>
#include <QBuffer>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

NetworkManager *NetworkManager::s_instance = nullptr;

NetworkManager *NetworkManager::instance()//获取单例实例
{
    if (!s_instance)
        s_instance = new NetworkManager;
    return s_instance;
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    m_worker = new NetworkWorker;//创建工作线程对象
    m_worker->moveToThread(&m_workerThread);//将线程移动到新线程

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);//线程结束时删除线程对象
    connect(m_worker, &NetworkWorker::connected,    this, &NetworkManager::connected);//连接信号和槽函数
    connect(m_worker, &NetworkWorker::disconnected, this, &NetworkManager::disconnected);//连接信号和槽函数
    connect(m_worker, &NetworkWorker::remoteCommand, this, &NetworkManager::remoteCommand);//连接信号和槽函数
    connect(m_worker, &NetworkWorker::statusMessage, this, &NetworkManager::statusMessage);//连接信号和槽函数

    m_workerThread.start();
}

void NetworkManager::start()
{
    SettingsManager *s = SettingsManager::instance();
    QMetaObject::invokeMethod(m_worker, "connectToMqtt",Qt::QueuedConnection, Q_ARG(QString, s->mqttHost()),Q_ARG(int, s->mqttPort()));//使用队列连接方式调用工作线程的connectToMqtt函数，传递MQTT主机和端口参数
    QMetaObject::invokeMethod(m_worker, "startHttpServer",Qt::QueuedConnection, Q_ARG(int, httpPort()));//使用队列连接方式调用工作线程的startHttpServer函数，传递HTTP端口参数
}

void NetworkManager::stop()
{
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);//使用队列连接方式调用工作线程的stop函数
    m_workerThread.quit();
    m_workerThread.wait();
}

void NetworkManager::publishStatus(const QJsonObject &status)//发布状态
{
    QMetaObject::invokeMethod(m_worker, "publishStatus",
        Qt::QueuedConnection, Q_ARG(QJsonObject, status));
}

void NetworkManager::startStream()//启动流
{
    QMetaObject::invokeMethod(m_worker, "startStream", Qt::QueuedConnection);
}

void NetworkManager::stopStream()//停止流
{
    QMetaObject::invokeMethod(m_worker, "stopStream", Qt::BlockingQueuedConnection);
}

bool NetworkManager::isStreaming() const//检查是否正在流
{
    bool result = false;
    QMetaObject::invokeMethod(m_worker, "isStreaming", Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, result));//使用阻塞队列连接方式调用工作线程的isStreaming函数，返回流状态
    return result;
}

void NetworkManager::setAppRunning(bool running)
{
    QMetaObject::invokeMethod(m_worker, "setAppRunning", Qt::QueuedConnection,
        Q_ARG(bool, running));
}

NetworkWorker::NetworkWorker(QObject *parent)//构造函数
    : QObject(parent), m_mqttSocket(nullptr), m_httpServer(nullptr),
      m_port(1883), m_connected(false),
      m_cameraFd(-1), m_cameraBuffers(nullptr), m_cameraBufCount(0),
      m_pixelFormat(0), m_streamWidth(0), m_streamHeight(0), m_streaming(false),
      m_appRunning(false)
{
    m_reconnectTimer = new QTimer(this);//创建重连定时器
    m_reconnectTimer->setInterval(5000);//设置重连定时器间隔为5秒
    connect(m_reconnectTimer, &QTimer::timeout, this, &NetworkWorker::onReconnectTimer);//连接重连定时器的超时信号到onReconnectTimer槽函数

    m_pingTimer = new QTimer(this);//创建Ping定时器
    m_pingTimer->setInterval(30000);//设置Ping定时器间隔为30秒
    connect(m_pingTimer, &QTimer::timeout, this, [this]() {//连接Ping定时器的超时信号到lambda函数
        if (m_connected && m_mqttSocket) 
        {
            QByteArray ping = buildMqttPacket(0xC0, QByteArray());//构建PINGREQ报文
            m_mqttSocket->write(ping);//发送PINGREQ报文
            m_mqttSocket->flush();
        }
    });

    m_streamTimer = new QTimer(this);
    m_streamTimer->setInterval(66);//设置流定时器间隔为66毫秒
    connect(m_streamTimer, &QTimer::timeout, this, &NetworkWorker::onStreamTimer);//连接流定时器的超时信号到onStreamTimer槽函数
}

void NetworkWorker::connectToMqtt(const QString &host, int port)
{
    m_host = host;//设置MQTT主机
    m_port = port;//设置MQTT端口

    m_mqttSocket = new QTcpSocket(this);//创建MQTT套接字，真正的连接将在后续步骤中进行
    connect(m_mqttSocket, &QTcpSocket::connected,
            this, &NetworkWorker::onMqttConnected);//连接MQTT套接字的已连接信号到onMqttConnected槽函数
    connect(m_mqttSocket, &QTcpSocket::disconnected,
            this, &NetworkWorker::onMqttDisconnected);//连接MQTT套接字的已断开信号到onMqttDisconnected槽函数
    connect(m_mqttSocket, &QTcpSocket::readyRead,
            this, &NetworkWorker::onMqttReadyRead);//连接MQTT套接字的可读信号到onMqttReadyRead槽函数

    qDebug() << "MQTT: 正在连接" << host << ":" << port;
    m_mqttSocket->connectToHost(host, port);
}

void NetworkWorker::onMqttConnected()
{
    qDebug() << "MQTT: TCP 已连接，发送 CONNECT 报文";
    sendMqttConnect();//发送CONNECT报文
}

void NetworkWorker::onMqttDisconnected()
{
    qDebug() << "MQTT: 连接已断开";
    m_connected = false;
    m_pingTimer->stop();//停止Ping定时器
    emit disconnected();
    emit statusMessage("MQTT 已断开,5秒后重连...");
    m_reconnectTimer->start();//启动重连定时器
}

void NetworkWorker::onReconnectTimer()
{
    if (!m_connected && m_mqttSocket) 
    {
        qDebug() << "MQTT: 尝试重连...";
        m_mqttSocket->connectToHost(m_host, m_port);
    }
}

QByteArray NetworkWorker::encodeRemainingLength(int length)
{
    QByteArray encoded;
    do 
    {
        unsigned char byte = length % 128;//计算剩余长度的低7位
        length /= 128;//计算剩余长度的高7位
        if (length > 0)
            byte |= 0x80;//设置最高位为1，表示还有更多字节
        encoded.append((char)byte);//将字节添加到编码数组中
    } 
    while (length > 0);
    return encoded;
}

QByteArray NetworkWorker::buildMqttPacket(unsigned char type, const QByteArray &payload)
{
    QByteArray pkt;//构建MQTT报文
    pkt.append((char)type);//添加消息类型字节
    pkt.append(encodeRemainingLength(payload.size()));//添加剩余长度字节
    pkt.append(payload);//添加有效载荷
    return pkt;
}

void NetworkWorker::sendMqttConnect()
{
    QByteArray variableHeader;
    variableHeader.append((char)0x00).append((char)0x04).append("MQTT");//协议名长度+内容
    variableHeader.append((char)0x04);//协议版本MQTT 3.1.1
    variableHeader.append((char)0x02);//清除会话(Clean Session)
    variableHeader.append((char)0x00).append((char)0x3C);//60秒心跳间隔

    QByteArray clientId = SettingsManager::instance()->deviceName().toUtf8();//获取设备名称
    m_subscribeTopic = "imx6ull/" + SettingsManager::instance()->deviceName() + "/cmd";

    QByteArray payload;//构建有效载荷
    payload.append(variableHeader);
    payload.append((char)(clientId.size() >> 8));
    payload.append((char)(clientId.size() & 0xFF));
    payload.append(clientId);

    QByteArray pkt = buildMqttPacket(0x10, payload);//构建CONNECT报文

    m_mqttSocket->write(pkt);//发送CONNECT报文
    m_mqttSocket->flush();
}

void NetworkWorker::sendMqttSubscribe(const QString &topic)//发送订阅请求
{
    QByteArray topicBytes = topic.toUtf8();//将主题转换为UTF-8字节数组
    QByteArray payload;//构建有效载荷
    payload.append((char)0x00).append((char)0x01);//   Packet ID: 0x00 0x01 (报文标识符)
    payload.append((char)(topicBytes.size() >> 8));
    payload.append((char)(topicBytes.size() & 0xFF));//主题长度
    payload.append(topicBytes);//主题内容
    payload.append((char)0x00);//QoS等级

    QByteArray pkt = buildMqttPacket(0x82, payload);

    m_mqttSocket->write(pkt);
}

void NetworkWorker::sendMqttPublish(const QString &topic, const QByteArray &payload)//发送发布消息
{
    QByteArray topicBytes = topic.toUtf8();
    QByteArray pktPayload;
    pktPayload.append((char)(topicBytes.size() >> 8));
    pktPayload.append((char)(topicBytes.size() & 0xFF));
    pktPayload.append(topicBytes);
    pktPayload.append(payload);

    QByteArray pkt = buildMqttPacket(0x30, pktPayload);

    m_mqttSocket->write(pkt);
    m_mqttSocket->flush();
}

void NetworkWorker::onMqttReadyRead()//处理MQTT套接字的可读信号
{
    QByteArray data = m_mqttSocket->readAll();//读取所有数据
    if (data.size() < 2) return;

    unsigned char type = data[0] & 0xF0;//获取消息类型

    if (type == 0x20) 
    {
        if (data.size() >= 4) 
        {
            unsigned char returnCode = data[3];//获取返回码
            if (returnCode == 0x00) 
            {
                qDebug() << "MQTT: CONNACK 连接成功";
                m_connected = true;
                m_reconnectTimer->stop();
                m_pingTimer->start();

                sendMqttSubscribe(m_subscribeTopic);

                emit connected();
                emit statusMessage("MQTT 已连接");
            } 
            else 
            {
                qWarning() << "MQTT: CONNACK 连接被拒绝，返回码=" << returnCode;
                m_mqttSocket->disconnectFromHost();
            }
        }
        return;
    }

    if (type == 0x90) 
    {
        qDebug() << "MQTT: SUBACK 订阅成功";
        return;
    }

    if (type == 0xD0) 
    {
        return;
    }

    if (type == 0x30) //处理PUBLISH消息
    {
        int pos = 1;
        int remaining = 0;//剩余长度
        int multiplier = 1;//乘数，用于计算剩余长度
        do {
            if (pos >= data.size()) return;//检查是否超出数据范围
            unsigned char byte = data[pos++];//获取当前字节
            remaining += (byte & 0x7F) * multiplier;//计算剩余长度
            multiplier *= 128;
            if ((byte & 0x80) == 0)//如果最高位为0，表示这是最后一个字节
                break;
        } while (true);//计算剩余长度

        int topicLen = (data[pos] << 8) | data[pos + 1];//获取主题长度
        pos += 2;//跳过主题长度字节
        QString topic = QString::fromUtf8(data.mid(pos, topicLen));//获取主题字符串
        pos += topicLen;//跳过主题字节

        QByteArray payload = data.mid(pos, remaining - 2 - topicLen);//获取有效载荷
        QJsonObject cmd = QJsonDocument::fromJson(payload).object();

        qDebug() << "MQTT: 收到命令 topic=" << topic;
        emit remoteCommand(topic, cmd);//发送远程命令信号
    }
}

void NetworkWorker::publishStatus(const QJsonObject &status)//发布状态消息
{
    if (!m_connected) return;

    QByteArray payload = QJsonDocument(status).toJson(QJsonDocument::Compact);//将状态对象转换为JSON字节数组
    QString topic = "imx6ull/" + SettingsManager::instance()->deviceName() + "/status";//构建主题
    sendMqttPublish(topic, payload);//发送发布消息
}

void NetworkWorker::startHttpServer(int port)
{
    m_httpServer = new QTcpServer(this);//创建HTTP服务器
    connect(m_httpServer, &QTcpServer::newConnection,this, &NetworkWorker::onNewHttpConnection);//连接新连接信号到槽函数

    if (m_httpServer->listen(QHostAddress::Any, port)) //监听HTTP服务器
    {
        qDebug() << "HTTP: 服务已启动，端口" << port;
        emit statusMessage(QString("HTTP 服务已启动，端口 %1").arg(port));
    } 
    else 
    {
        qWarning() << "HTTP: 启动失败" << m_httpServer->errorString();
    }
}

void NetworkWorker::onNewHttpConnection()//处理新HTTP连接
{
    while (m_httpServer->hasPendingConnections()) //处理所有待连接
    {
        QTcpSocket *client = m_httpServer->nextPendingConnection();//获取下一个待连接的客户端套接字
        m_httpClients.append(client);//添加到客户端列表
        connect(client, &QTcpSocket::readyRead,this, &NetworkWorker::onHttpReadyRead);
        connect(client, &QTcpSocket::disconnected, this, [this, client]() {
            m_httpClients.removeAll(client);
            client->deleteLater();
        });
    }
}

void NetworkWorker::onHttpReadyRead()//处理HTTP套接字的可读信号
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());//获取发送信号的套接字对象
    if (!client) return;

    QByteArray data = client->readAll();
    handleHttpRequest(client, data);//处理HTTP请求
}

void NetworkWorker::handleHttpRequest(QTcpSocket *client, const QByteArray &data)//处理HTTP请求
{
    QString request = QString::fromUtf8(data);//将请求数据转换为字符串

    if (request.contains("GET /stream")) 
    {
        m_streamClients.append(client);//添加到推流客户端列表
        connect(client, &QTcpSocket::disconnected, this, [this, client]() {
            m_streamClients.removeAll(client);//从客户端列表中移除
            if (m_streamClients.isEmpty())
            {
                m_streamTimer->stop();
                closeCamera();//关闭摄像头
                qDebug() << "STREAM: 所有客户端已断开，摄像头已关闭";
            }
        });

        if (m_cameraFd < 0)//如果摄像头被占用，尝试打开摄像头
        {
            if (!openCamera())
            {
                qWarning() << "STREAM: 打开摄像头失败";
                client->write(buildHttpResponse("Camera open failed", "text/plain"));
                client->flush();
                client->disconnectFromHost();
                m_streamClients.removeAll(client);
                return;
            }
        }

        if (!m_streamTimer->isActive())//如果定时器未激活，启动定时器
            m_streamTimer->start();

        QByteArray header = QString(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=mjpegframe\r\n"//设置内容类型为MIME混合替换，边界为mjpegframe
            "Connection: close\r\n"//关闭连接
            "Cache-Control: no-cache\r\n"//禁用缓存
            "\r\n").toUtf8();
        client->write(header);//发送HTTP头
        client->flush();
        qDebug() << "STREAM: 客户端已连接，开始推流";
        return;
    }

    if (request.contains("GET /api/status")) //处理状态请求
    {
        QJsonObject status;
        status["device"] = SettingsManager::instance()->deviceName();
        status["brightness"] = SettingsManager::instance()->brightness();//获取亮度设置

        QFile tempFile("/sys/class/thermal/thermal_zone0/temp");//读取CPU温度文件
        if (tempFile.open(QIODevice::ReadOnly))
        {
            int temp = tempFile.readAll().trimmed().toInt() / 1000;
            status["cpuTemp"] = temp;
        }
        status["streaming"] = m_streaming;//获取流状态
        status["appRunning"] = m_appRunning;//获取应用运行状态

        QByteArray json = QJsonDocument(status).toJson();
        client->write(buildHttpResponse(QString::fromUtf8(json), "application/json"));
    } 
    else if (request.contains("GET /api/brightness/up")) //处理亮度增加请求
    {
        int v = SettingsManager::instance()->brightness() + 1;
        if (v <= 7) 
        {
            SettingsManager::instance()->setBrightness(v);
            emit remoteCommand("set_brightness", {{"level", v}});
        }
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    } 
    else if (request.contains("GET /api/brightness/down")) //处理亮度减少请求
    {
        int v = SettingsManager::instance()->brightness() - 1;
        if (v >= 0) 
        {
            SettingsManager::instance()->setBrightness(v);
            emit remoteCommand("set_brightness", {{"level", v}});
        }
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/brightness/set"))
    {
        int level = -1;
        QRegularExpression re("level=(\\d)");
        QRegularExpressionMatch m = re.match(request);
        if (m.hasMatch())
        {
            level = m.captured(1).toInt();
            if (level >= 0 && level <= 7)
            {
                SettingsManager::instance()->setBrightness(level);
                emit remoteCommand("set_brightness", {{"level", level}});
            }
        }
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/start_stream"))
    {
        emit remoteCommand("start_stream", {});//开始推流
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/stop_stream"))
    {
        emit remoteCommand("stop_stream", {});
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/app/launch"))//处理应用启动请求
    {
        QString name = "camera";
        int idx = request.indexOf("name=");//获取应用名称参数索引
        if (idx >= 0)
        {
            name = request.mid(idx + 5);//获取应用名称
            int end = name.indexOf(' ');
            if (end < 0) end = name.indexOf('\r');
            if (end < 0) end = name.indexOf('\n');
            if (end >= 0) name = name.left(end);
        }
        emit remoteCommand("launch_app", {{"name", name}});
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/app/stop"))
    {
        emit remoteCommand("stop_app", {});
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/reboot"))
    {
        emit remoteCommand("reboot", {});
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else if (request.contains("GET /api/shutdown"))
    {
        emit remoteCommand("shutdown", {});
        client->write(buildHttpResponse("{\"ok\":true}", "application/json"));
    }
    else 
    {
        QString html = QString(
            "<!DOCTYPE html><html lang=\"zh\"><head>"
            "<meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>i.MX6ULL Dashboard</title>"
            "<style>"
            "*{margin:0;padding:0;box-sizing:border-box}"
            "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
            "background:linear-gradient(135deg,#0f0f23 0%,#1a1a3e 50%,#0d1117 100%);"
            "color:#e0e0e0;min-height:100vh;padding:16px}"
            ".header{text-align:center;padding:20px 0 16px}"
            ".header h1{font-size:22px;font-weight:700;"
            "background:linear-gradient(90deg,#FFD54F,#FF9800);"
            "-webkit-background-clip:text;-webkit-text-fill-color:transparent;"
            "background-clip:text}"
            ".header .sub{font-size:12px;color:#666;margin-top:4px}"
            ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px;max-width:600px;margin:0 auto}"
            ".card{background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.08);"
            "border-radius:14px;padding:14px;text-align:center;"
            "backdrop-filter:blur(8px);transition:all .2s}"
            ".card:hover{background:rgba(255,255,255,0.07);border-color:rgba(255,255,255,0.14)}"
            ".card .icon{font-size:26px;margin-bottom:6px}"
            ".card .label{font-size:11px;color:#888;text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px}"
            ".card .value{font-size:20px;font-weight:700;color:#fff}"
            ".card .subval{font-size:11px;color:#666;margin-top:2px}"
            ".section{max-width:600px;margin:14px auto}"
            ".section-title{font-size:13px;color:#666;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px;padding-left:4px}"
            ".btn-row{display:flex;gap:8px;flex-wrap:wrap}"
            ".btn{flex:1;min-width:80px;padding:12px 8px;border:none;border-radius:10px;"
            "font-size:13px;font-weight:600;cursor:pointer;transition:all .2s;"
            "color:#fff;text-align:center;display:flex;align-items:center;justify-content:center;gap:4px}"
            ".btn:active{transform:scale(.95)}"
            ".btn-primary{background:linear-gradient(135deg,#667eea,#764ba2)}"
            ".btn-primary:hover{box-shadow:0 4px 15px rgba(102,126,234,.4)}"
            ".btn-success{background:linear-gradient(135deg,#43e97b,#38f9d7);color:#111}"
            ".btn-success:hover{box-shadow:0 4px 15px rgba(67,233,123,.4)}"
            ".btn-warning{background:linear-gradient(135deg,#f093fb,#f5576c)}"
            ".btn-warning:hover{box-shadow:0 4px 15px rgba(240,147,251,.4)}"
            ".btn-danger{background:linear-gradient(135deg,#fa709a,#fee140);color:#111}"
            ".btn-danger:hover{box-shadow:0 4px 15px rgba(250,112,154,.4)}"
            ".btn-dark{background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.12)}"
            ".btn-dark:hover{background:rgba(255,255,255,.14)}"
            ".btn-dark.active{background:rgba(67,233,123,.2);border-color:rgba(67,233,123,.4);color:#43e97b}"
            ".brightness-bar{display:flex;align-items:center;gap:10px;background:rgba(255,255,255,0.04);"
            "border-radius:10px;padding:10px 14px;border:1px solid rgba(255,255,255,0.08)}"
            ".brightness-bar .icon{font-size:20px}"
            ".brightness-bar input[type=range]{flex:1;-webkit-appearance:none;height:6px;"
            "background:linear-gradient(90deg,#333,#FFD54F);border-radius:3px;outline:none}"
            ".brightness-bar input[type=range]::-webkit-slider-thumb{"
            "-webkit-appearance:none;width:22px;height:22px;background:#FFD54F;"
            "border-radius:50%;cursor:pointer;box-shadow:0 0 10px rgba(255,213,79,.5)}"
            ".brightness-bar .val{font-size:18px;font-weight:700;color:#FFD54F;min-width:24px;text-align:center}"
            ".stream-box{position:relative;border-radius:14px;overflow:hidden;"
            "border:1px solid rgba(255,255,255,0.08);background:#000;min-height:200px;"
            "display:flex;align-items:center;justify-content:center}"
            ".stream-box img{width:100%;display:block}"
            ".stream-box .placeholder{color:#444;font-size:14px}"
            ".stream-toggle{position:absolute;top:8px;right:8px;z-index:2;"
            "padding:8px 14px;border-radius:20px;border:none;font-size:11px;font-weight:600;"
            "cursor:pointer;transition:all .2s;color:#fff}"
            ".stream-toggle.on{background:rgba(76,175,80,.8)}"
            ".stream-toggle.off{background:rgba(158,158,158,.6)}"
            ".toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);"
            "background:rgba(0,0,0,.85);color:#fff;padding:10px 24px;border-radius:20px;"
            "font-size:13px;z-index:99;opacity:0;transition:opacity .3s;pointer-events:none}"
            ".toast.show{opacity:1}"
            ".status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}"
            ".status-dot.online{background:#43e97b;box-shadow:0 0 6px rgba(67,233,123,.6)}"
            ".status-dot.offline{background:#f44336;box-shadow:0 0 6px rgba(244,67,54,.6)}"
            "@media(max-width:400px){.grid{grid-template-columns:repeat(2,1fr)}"
            ".header h1{font-size:18px}.card .value{font-size:17px}}"
            "</style></head><body>"
            "<div class=\"header\">"
            "<h1>i.MX6ULL 智能终端</h1>"
            "<div class=\"sub\"><span id=\"netDot\" class=\"status-dot online\"></span><span id=\"devName\">--</span></div>"
            "</div>"
            "<div class=\"grid\">"
            "<div class=\"card\"><div class=\"icon\">☀️</div><div class=\"label\">亮度</div><div class=\"value\" id=\"brightness\">--</div></div>"
            "<div class=\"card\"><div class=\"icon\">🌡️</div><div class=\"label\">CPU温度</div><div class=\"value\" id=\"cpuTemp\">--°C</div></div>"
            "<div class=\"card\"><div class=\"icon\">📡</div><div class=\"label\">推流</div><div class=\"value\" id=\"streamStatus\">--</div></div>"
            "<div class=\"card\"><div class=\"icon\">📱</div><div class=\"label\">应用状态</div><div class=\"value\" id=\"appStatus\">空闲</div></div>"
            "</div>"
            "<div class=\"section\"><div class=\"section-title\">亮度调节</div>"
            "<div class=\"brightness-bar\">"
            "<span class=\"icon\">🔅</span>"
            "<input type=\"range\" min=\"0\" max=\"7\" value=\"0\" id=\"brightSlider\" oninput=\"onSliderChange(this.value)\">"
            "<span class=\"icon\">🔆</span>"
            "<span class=\"val\" id=\"brightVal\">0</span>"
            "</div></div>"
            "<div class=\"section\"><div class=\"section-title\">应用控制</div>"
            "<div class=\"btn-row\">"
            "<button class=\"btn btn-primary\" onclick=\"launchApp('camera')\">📷 相机</button>"
            "<button class=\"btn btn-primary\" onclick=\"launchApp('audio')\">🎵 音乐</button>"
            "<button class=\"btn btn-dark\" onclick=\"launchApp('game')\">🎮 游戏</button>"
            "</div>"
            "<div class=\"btn-row\" style=\"margin-top:6px\">"
            "<button class=\"btn btn-dark\" onclick=\"launchApp('filemanager')\">📁 文件</button>"
            "<button class=\"btn btn-dark\" onclick=\"launchApp('terminal')\">💻 终端</button>"
            "<button class=\"btn btn-dark\" onclick=\"launchApp('network')\">⚙️ 设置</button>"
            "</div>"
            "<div class=\"btn-row\" style=\"margin-top:6px\">"
            "<button class=\"btn btn-warning\" onclick=\"stopApp()\">⏹ 停止应用</button>"
            "</div></div>"
            "<div class=\"section\"><div class=\"section-title\">实时画面</div>"
            "<div class=\"stream-box\" id=\"streamBox\">"
            "<button class=\"stream-toggle on\" id=\"streamBtn\" onclick=\"toggleStream()\">● 推流中</button>"
            "<img id=\"streamImg\" src=\"/stream\" onerror=\"onStreamError()\" onload=\"onStreamLoad()\" />"
            "<span class=\"placeholder\" id=\"streamPlaceholder\">点击下方按钮开启推流</span>"
            "</div></div>"
            "<div class=\"section\"><div class=\"section-title\">系统控制</div>"
            "<div class=\"btn-row\">"
            "<button class=\"btn btn-danger\" onclick=\"reboot()\">🔄 重启</button>"
            "<button class=\"btn btn-dark\" onclick=\"shutdown()\">⏻ 关机</button>"
            "</div></div>"
            
            "<div style=\"text-align:center;padding:20px;color:#444;font-size:10px\">"
            "i.MX6ULL Embedded System v1.0</div>"
            "<div class=\"toast\" id=\"toast\"></div>"
            "<script>"
            "var streamOn=true;"
            "function $(id){return document.getElementById(id)}"
            "function toast(msg){var t=$('toast');t.textContent=msg;t.classList.add('show');"
            "setTimeout(function(){t.classList.remove('show')},2000)}"
            "function toggleStream(){"
            "var img=$('streamImg'),btn=$('streamBtn'),ph=$('streamPlaceholder');"
            "if(streamOn){"
            "img.src='';img.style.display='none';ph.style.display='block';"
            "btn.textContent='○ 已关闭';btn.className='stream-toggle off';streamOn=false;"
            "fetch('/api/stop_stream');toast('推流已关闭')"
            "}else{"
            "img.src='/stream?t='+Date.now();img.style.display='block';ph.style.display='none';"
            "btn.textContent='● 推流中';btn.className='stream-toggle on';streamOn=true;"
            "fetch('/api/start_stream');toast('推流已开启')"
            "}}"
            "function onStreamError(){"
            "var img=$('streamImg'),ph=$('streamPlaceholder'),btn=$('streamBtn');"
            "img.style.display='none';ph.style.display='block';ph.textContent='摄像头不可用';"
            "btn.textContent='○ 已关闭';btn.className='stream-toggle off';streamOn=false"
            "}"
            "function onStreamLoad(){"
            "$('streamPlaceholder').style.display='none';$('streamImg').style.display='block'"
            "}"
            "function onSliderChange(v){"
            "$('brightVal').textContent=v;"
            "fetch('/api/brightness/set?level='+v).then(function(r){return r.json()}).then(function(){fetchStatus()})"
            "}"
            "function launchApp(name){fetch('/api/app/launch?name='+name).then(function(r){return r.json()}).then(function(){"
            "toast('已启动 '+name);fetchStatus()})}"
            "function stopApp(){fetch('/api/app/stop').then(function(r){return r.json()}).then(function(){"
            "toast('已停止应用');fetchStatus()})}"
            "function reboot(){if(confirm('确定要重启设备吗？')){fetch('/api/reboot');toast('设备正在重启...')}}"
            "function shutdown(){if(confirm('确定要关机吗？')){fetch('/api/shutdown');toast('设备正在关机...')}}"
            
            "function fetchStatus(){"
            "fetch('/api/status').then(function(r){return r.json()}).then(function(s){"
            "$('devName').textContent=s.device;"
            "$('brightness').textContent='Lv.'+s.brightness;"
            "$('brightSlider').value=s.brightness;"
            "$('brightVal').textContent=s.brightness;"
            "$('cpuTemp').textContent=s.cpuTemp+'°C';"
            "$('streamStatus').textContent=s.streaming?'推送中':'已关闭';"
            "$('streamStatus').style.color=s.streaming?'#43e97b':'#888';"
            "$('appStatus').textContent=s.appRunning?'运行中':'空闲';"
            "$('appStatus').style.color=s.appRunning?'#FF9800':'#fff';"
            "if(s.streaming&&!streamOn){streamOn=true;$('streamImg').src='/stream?t='+Date.now();"
            "$('streamImg').style.display='block';$('streamPlaceholder').style.display='none';"
            "$('streamBtn').textContent='● 推流中';$('streamBtn').className='stream-toggle on'}"
            "if(!s.streaming&&streamOn){streamOn=false;$('streamImg').src='';"
            "$('streamImg').style.display='none';$('streamPlaceholder').style.display='block';"
            "$('streamBtn').textContent='○ 已关闭';$('streamBtn').className='stream-toggle off'}"
            "}).catch(function(e){console.log(e)})}"
            "fetchStatus();setInterval(fetchStatus,3000);"
            "</script></body></html>");

        client->write(buildHttpResponse(html));//发送HTML响应
    }
    client->flush();
    client->disconnectFromHost();//断开连接
}

QByteArray NetworkWorker::buildHttpResponse(const QString &body, const QString &contentType)//构建HTTP响应
{
    QByteArray content = body.toUtf8();//将响应体转换为字节数组
    QString header = QString(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %1; charset=utf-8\r\n"
        "Content-Length: %2\r\n"
        "Connection: close\r\n"
        "\r\n").arg(contentType, QString::number(content.size()));
    return header.toUtf8() + content;
}

bool NetworkWorker::openCamera()
{
    QMutexLocker locker(&m_cameraMutex);//加锁，确保线程安全

    const char *devices[] = {"/dev/video1", "/dev/video0"};//尝试打开摄像头设备
    for (const char *dev : devices)
    {
        m_cameraFd = open(dev, O_RDWR | O_NONBLOCK);
        if (m_cameraFd >= 0)
        {
            qDebug() << "CAMERA: 已打开" << dev;
            break;
        }
        qWarning() << "CAMERA: 无法打开" << dev << ":" << strerror(errno);
    }

    if (m_cameraFd < 0)
        return false;

    struct v4l2_capability cap;
    if (ioctl(m_cameraFd, VIDIOC_QUERYCAP, &cap) < 0)//查询摄像头设备能力
    {
        qWarning() << "CAMERA: VIDIOC_QUERYCAP 失败";//查询摄像头设备能力失败
        close(m_cameraFd);
        m_cameraFd = -1;
        return false;
    }

    struct v4l2_format fmt;//设置视频格式
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//设置视频格式为视频捕获

    struct { int w; int h; } resolutions[] = 
    {
        {640, 480}, {320, 240}, {480, 272}
    };

    bool formatOk = false;
    for (auto &res : resolutions)
    {
        fmt.fmt.pix.width = res.w;
        fmt.fmt.pix.height = res.h;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;//设置视频格式为任意场扫描

        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        if (ioctl(m_cameraFd, VIDIOC_S_FMT, &fmt) == 0)
        {
            m_pixelFormat = V4L2_PIX_FMT_MJPEG;
            formatOk = true;
            qDebug() << "CAMERA: MJPEG" << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height;
            break;
        }

        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if (ioctl(m_cameraFd, VIDIOC_S_FMT, &fmt) == 0)
        {
            m_pixelFormat = V4L2_PIX_FMT_YUYV;
            formatOk = true;
            qDebug() << "CAMERA: YUYV" << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height;
            break;
        }
    }

    if (!formatOk)
    {
        qWarning() << "CAMERA: VIDIOC_S_FMT 所有格式/分辨率均失败";
        close(m_cameraFd);
        m_cameraFd = -1;
        return false;
    }

    m_streamWidth = fmt.fmt.pix.width;
    m_streamHeight = fmt.fmt.pix.height;

    struct v4l2_requestbuffers req;//请求缓冲区
    memset(&req, 0, sizeof(req));//清零结构体
    req.count = 4;//请求4个缓冲区
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//设置缓冲区类型为视频捕获
    req.memory = V4L2_MEMORY_MMAP;//设置缓冲区内存类型为内存映射

    if (ioctl(m_cameraFd, VIDIOC_REQBUFS, &req) < 0)//请求缓冲区
    {
        qWarning() << "CAMERA: VIDIOC_REQBUFS 失败";
        close(m_cameraFd);
        m_cameraFd = -1;
        return false;
    }

    m_cameraBufCount = req.count;
    m_cameraBuffers = new V4L2Buffer[m_cameraBufCount];

    for (int i = 0; i < m_cameraBufCount; i++)//遍历缓冲区
    {
        struct v4l2_buffer buf;//查询缓冲区
        memset(&buf, 0, sizeof(buf));//清零结构体
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//设置缓冲区类型为视频捕获
        buf.memory = V4L2_MEMORY_MMAP;//设置缓冲区内存类型为内存映射
        buf.index = i;

        if (ioctl(m_cameraFd, VIDIOC_QUERYBUF, &buf) < 0)//查询缓冲区
        {
            qWarning() << "CAMERA: VIDIOC_QUERYBUF 失败";
            close(m_cameraFd);
            m_cameraFd = -1;
            return false;
        }

        m_cameraBuffers[i].length = buf.length;//保存缓冲区长度
        m_cameraBuffers[i].start = mmap(nullptr, buf.length,PROT_READ | PROT_WRITE, MAP_SHARED, m_cameraFd, buf.m.offset);//映射缓冲区到用户空间

        if (m_cameraBuffers[i].start == MAP_FAILED)//映射缓冲区失败
        {
            qWarning() << "CAMERA: mmap 失败";
            close(m_cameraFd);
            m_cameraFd = -1;
            return false;
        }
    }

    for (int i = 0; i < m_cameraBufCount; i++)//将缓冲区入队
    {
        struct v4l2_buffer buf;//查询缓冲区
        memset(&buf, 0, sizeof(buf));//清零结构体
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//设置缓冲区类型为视频捕获
        buf.memory = V4L2_MEMORY_MMAP;//设置缓冲区内存类型为内存映射
        buf.index = i;

        if (ioctl(m_cameraFd, VIDIOC_QBUF, &buf) < 0)
        {
            qWarning() << "CAMERA: VIDIOC_QBUF 失败";
            close(m_cameraFd);
            m_cameraFd = -1;
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//设置缓冲区类型为视频捕获
    if (ioctl(m_cameraFd, VIDIOC_STREAMON, &type) < 0)//开启流模式
    {
        qWarning() << "CAMERA: VIDIOC_STREAMON 失败";
        close(m_cameraFd);
        m_cameraFd = -1;
        return false;
    }

    qDebug() << "CAMERA: 摄像头已打开，开始采集";
    m_streaming = true;
    return true;
}

void NetworkWorker::closeCamera()
{
    QMutexLocker locker(&m_cameraMutex);

    if (m_cameraFd >= 0)
    {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(m_cameraFd, VIDIOC_STREAMOFF, &type);

        for (int i = 0; i < m_cameraBufCount; i++)
            munmap(m_cameraBuffers[i].start, m_cameraBuffers[i].length);

        delete[] m_cameraBuffers;
        m_cameraBuffers = nullptr;
        m_cameraBufCount = 0;

        close(m_cameraFd);
        m_cameraFd = -1;
        m_streaming = false;
        qDebug() << "CAMERA: 摄像头已关闭";
    }
}

bool NetworkWorker::grabFrame(QByteArray &jpegData)//获取一帧视频数据
{
    QMutexLocker locker(&m_cameraMutex);

    if (m_cameraFd < 0)
        return false;

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//设置缓冲区类型为视频捕获
    buf.memory = V4L2_MEMORY_MMAP;//设置缓冲区内存类型为内存映射

    if (ioctl(m_cameraFd, VIDIOC_DQBUF, &buf) < 0)//从队列中获取缓冲区
    {
        if (errno == EAGAIN)
            return false;
        qWarning() << "CAMERA: VIDIOC_DQBUF 失败:" << strerror(errno);
        return false;
    }

    QByteArray rawData((char *)m_cameraBuffers[buf.index].start, buf.bytesused);//将缓冲区数据转换为QByteArray

    if (m_pixelFormat == V4L2_PIX_FMT_MJPEG)
    {
        jpegData = rawData;
    }
    else
    {
        QImage image(m_streamWidth, m_streamHeight, QImage::Format_RGB32);
        const unsigned char *yuyv = (const unsigned char *)rawData.constData();
        unsigned char *rgb = image.bits();
        int total = m_streamWidth * m_streamHeight;

        for (int i = 0; i < total; i += 2)
        {
            int y0 = yuyv[i * 2];
            int u  = yuyv[i * 2 + 1] - 128;
            int y1 = yuyv[i * 2 + 2];
            int v  = yuyv[i * 2 + 3] - 128;

            int r0 = y0 + ((359 * v) >> 8);
            int g0 = y0 - ((88 * u + 183 * v) >> 8);
            int b0 = y0 + ((454 * u) >> 8);

            int r1 = y1 + ((359 * v) >> 8);
            int g1 = y1 - ((88 * u + 183 * v) >> 8);
            int b1 = y1 + ((454 * u) >> 8);

            auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
            rgb[i * 4 + 0] = clamp(b0);
            rgb[i * 4 + 1] = clamp(g0);
            rgb[i * 4 + 2] = clamp(r0);
            rgb[i * 4 + 3] = 0xff;
            rgb[i * 4 + 4] = clamp(b1);
            rgb[i * 4 + 5] = clamp(g1);
            rgb[i * 4 + 6] = clamp(r1);
            rgb[i * 4 + 7] = 0xff;
        }

        QBuffer buffer;
        image.save(&buffer, "JPEG", 70);
        jpegData = buffer.data();
    }

    ioctl(m_cameraFd, VIDIOC_QBUF, &buf);//将缓冲区入队

    return true;
}

void NetworkWorker::onStreamTimer()//推流定时器回调
{
    if (m_streamClients.isEmpty())
        return;

    QByteArray jpegData;
    if (!grabFrame(jpegData))
        return;

    if (jpegData.isEmpty())
        return;

    QByteArray frame;
    frame.append("--mjpegframe\r\n");
    frame.append("Content-Type: image/jpeg\r\n");
    frame.append("Content-Length: " + QByteArray::number(jpegData.size()) + "\r\n");
    frame.append("\r\n");
    frame.append(jpegData);
    frame.append("\r\n");

    for (int i = m_streamClients.size() - 1; i >= 0; i--)//遍历所有推流客户端
    {
        QTcpSocket *client = m_streamClients[i];
        if (client->state() == QAbstractSocket::ConnectedState)
        {
            client->write(frame);
            client->flush();
        }
        else
        {
            m_streamClients.removeAt(i);
        }
    }

    if (m_streamClients.isEmpty())
    {
        m_streamTimer->stop();
        closeCamera();
        qDebug() << "STREAM: 所有客户端已断开，摄像头已关闭";
    }
}

void NetworkWorker::startStream()
{
    if (m_cameraFd < 0)
    {
        if (!openCamera())
        {
            qWarning() << "STREAM: 启动推流失败，无法打开摄像头";
            return;
        }
    }
    if (!m_streamTimer->isActive())
    {
        m_streamTimer->start();
    }
    qDebug() << "STREAM: 推流已启动";
}

void NetworkWorker::stopStream()
{
    m_streamTimer->stop();
    for (QTcpSocket *c : m_streamClients)
    {
        c->disconnectFromHost();
        c->deleteLater();
    }
    m_streamClients.clear();
    closeCamera();
    qDebug() << "STREAM: 推流已强制停止";
}

void NetworkWorker::stop()
{
    m_reconnectTimer->stop();
    m_pingTimer->stop();
    m_streamTimer->stop();
    closeCamera();

    if (m_mqttSocket) 
    {
        m_mqttSocket->disconnectFromHost();
        m_mqttSocket->deleteLater();
        m_mqttSocket = nullptr;
    }
    if (m_httpServer) 
    {
        m_httpServer->close();
        m_httpServer->deleteLater();
        m_httpServer = nullptr;
    }
    for (QTcpSocket *c : m_httpClients)
        c->deleteLater();
    m_httpClients.clear();
    for (QTcpSocket *c : m_streamClients)
        c->deleteLater();
    m_streamClients.clear();
}