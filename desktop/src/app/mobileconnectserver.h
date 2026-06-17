#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

#include <functional>

class QJsonObject;
class QTcpServer;
class QTcpSocket;
class QTimer;

namespace SmTool::App {

class AppController;

class MobileConnectServer : public QObject
{
    Q_OBJECT

public:
    using ConfirmationHandler = std::function<bool(const QString &)>;

    explicit MobileConnectServer(AppController *controller, QObject *parent = nullptr);

    void applySettings(bool enabled, const QString &listenIp, quint16 port);
    void setEnabled(bool enabled);
    void setListenIp(const QString &listenIp);
    void setPort(quint16 port);
    void setConfirmationHandler(ConfirmationHandler handler);

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] quint16 listeningPort() const;

private:
    enum class ConnectionState {
        Closed,
        WaitHello,
        WaitPayload,
    };

    void restartIfNeeded();
    void start();
    void stop();
    void closeActiveConnection();
    void handleNewConnection();
    void handleReadyRead();
    void handleDisconnected();
    void handleTimeout();
    bool processHelloMessage(const QJsonObject &message);
    bool processPayloadMessage(const QJsonObject &message);
    void sendErrorAndClose(const QString &message);
    void sendJsonMessage(const QJsonObject &message);
    void sendJsonMessageAndClose(const QJsonObject &message);
    [[nodiscard]] bool validateHelloMessage(const QJsonObject &message, QString *code, QString *errorMessage) const;
    [[nodiscard]] QString normalizeListenIp(const QString &listenIp) const;

    static constexpr qsizetype maxMessageSizeBytes_ = 4 * 1024 * 1024;
    static constexpr int helloTimeoutMs_ = 5000;
    static constexpr int payloadTimeoutMs_ = 30000;

    AppController *controller_ = nullptr;
    QTcpServer *server_ = nullptr;
    QTcpSocket *activeSocket_ = nullptr;
    QTimer *timeoutTimer_ = nullptr;
    QByteArray readBuffer_;
    ConnectionState connectionState_ = ConnectionState::Closed;
    bool enabled_ = false;
    QString listenIp_ = QStringLiteral("0.0.0.0");
    quint16 port_ = 45437;
    QString pendingCode_;
    ConfirmationHandler confirmationHandler_;
};

} // namespace SmTool::App
