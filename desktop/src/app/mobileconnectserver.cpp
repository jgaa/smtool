#include "app/mobileconnectserver.h"

#include "app/appcontroller.h"
#include "app/loggingcontroller.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::App {
namespace {

QString formattedCode(const QString &code)
{
    return code.size() == 8 ? code.sliced(0, 4) + u'-' + code.sliced(4, 4) : code;
}

} // namespace

MobileConnectServer::MobileConnectServer(AppController *controller, QObject *parent)
    : QObject(parent)
    , controller_(controller)
    , server_(new QTcpServer(this))
    , timeoutTimer_(new QTimer(this))
{
    timeoutTimer_->setSingleShot(true);

    connect(server_, &QTcpServer::newConnection, this, &MobileConnectServer::handleNewConnection);
    connect(timeoutTimer_, &QTimer::timeout, this, &MobileConnectServer::handleTimeout);
}

void MobileConnectServer::applySettings(bool enabled, const QString &listenIp, quint16 port)
{
    listenIp_ = normalizeListenIp(listenIp);
    port_ = port;
    enabled_ = enabled;
    restartIfNeeded();
}

void MobileConnectServer::setEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;
    restartIfNeeded();
}

void MobileConnectServer::setListenIp(const QString &listenIp)
{
    const auto normalized = normalizeListenIp(listenIp);
    if (listenIp_ == normalized) {
        return;
    }

    listenIp_ = normalized;
    restartIfNeeded();
}

void MobileConnectServer::setPort(quint16 port)
{
    if (port_ == port) {
        return;
    }

    port_ = port;
    restartIfNeeded();
}

void MobileConnectServer::setConfirmationHandler(ConfirmationHandler handler)
{
    confirmationHandler_ = std::move(handler);
}

bool MobileConnectServer::isRunning() const
{
    return server_->isListening();
}

quint16 MobileConnectServer::listeningPort() const
{
    return server_->serverPort();
}

void MobileConnectServer::restartIfNeeded()
{
    if (!enabled_) {
        stop();
        return;
    }

    start();
}

void MobileConnectServer::start()
{
    stop();

    QHostAddress listenAddress;
    if (!listenAddress.setAddress(listenIp_)) {
        const auto message = QStringLiteral("Mobile Connect listen IP is invalid: %1").arg(listenIp_);
        LOG_ERROR << message.toStdString();
        if (controller_ != nullptr) {
            controller_->reportStatusMessage(message);
        }
        return;
    }

    if (!server_->listen(listenAddress, port_)) {
        const auto message = QStringLiteral("Mobile Connect could not listen on %1:%2: %3")
                                 .arg(listenIp_)
                                 .arg(port_)
                                 .arg(server_->errorString());
        LOG_ERROR << message.toStdString();
        if (controller_ != nullptr) {
            controller_->reportStatusMessage(message);
        }
        return;
    }

    const auto message = QStringLiteral("Mobile Connect listening on %1:%2")
                             .arg(server_->serverAddress().toString())
                             .arg(server_->serverPort());
    LOG_INFO << message.toStdString();
    if (controller_ != nullptr) {
        controller_->reportStatusMessage(message);
    }
}

void MobileConnectServer::stop()
{
    closeActiveConnection();
    if (!server_->isListening()) {
        return;
    }

    const auto message = QStringLiteral("Mobile Connect stopped.");
    LOG_INFO << message.toStdString();
    server_->close();
    if (controller_ != nullptr) {
        controller_->reportStatusMessage(message);
    }
}

void MobileConnectServer::closeActiveConnection()
{
    timeoutTimer_->stop();
    readBuffer_.clear();
    pendingCode_.clear();
    connectionState_ = ConnectionState::Closed;

    if (activeSocket_ == nullptr) {
        return;
    }

    disconnect(activeSocket_, nullptr, this, nullptr);
    activeSocket_->abort();
    activeSocket_->deleteLater();
    activeSocket_ = nullptr;
}

void MobileConnectServer::handleNewConnection()
{
    while (server_->hasPendingConnections()) {
        auto *socket = server_->nextPendingConnection();
        if (activeSocket_ != nullptr) {
            QJsonObject busyMessage{
                {QStringLiteral("type"), QStringLiteral("error")},
                {QStringLiteral("message"), QStringLiteral("Server busy")},
            };
            const auto payload = QJsonDocument{busyMessage}.toJson(QJsonDocument::Compact) + '\n';
            socket->write(payload);
            socket->waitForBytesWritten(1000);
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        activeSocket_ = socket;
        readBuffer_.clear();
        pendingCode_.clear();
        connectionState_ = ConnectionState::WaitHello;

        connect(activeSocket_, &QTcpSocket::readyRead, this, &MobileConnectServer::handleReadyRead);
        connect(activeSocket_, &QTcpSocket::disconnected, this, &MobileConnectServer::handleDisconnected);

        timeoutTimer_->start(helloTimeoutMs_);
    }
}

void MobileConnectServer::handleReadyRead()
{
    if (activeSocket_ == nullptr) {
        return;
    }

    readBuffer_.append(activeSocket_->readAll());
    if (readBuffer_.size() > maxMessageSizeBytes_) {
        sendErrorAndClose(QStringLiteral("Message too large"));
        return;
    }

    while (true) {
        const auto newlineIndex = readBuffer_.indexOf('\n');
        if (newlineIndex < 0) {
            return;
        }

        const auto line = readBuffer_.first(newlineIndex);
        readBuffer_.remove(0, newlineIndex + 1);

        if (line.size() > maxMessageSizeBytes_) {
            sendErrorAndClose(QStringLiteral("Message too large"));
            return;
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            sendErrorAndClose(QStringLiteral("Invalid JSON"));
            return;
        }

        const auto message = document.object();
        if (connectionState_ == ConnectionState::WaitHello) {
            if (!processHelloMessage(message)) {
                return;
            }
            continue;
        }

        if (connectionState_ == ConnectionState::WaitPayload) {
            processPayloadMessage(message);
            return;
        }

        sendErrorAndClose(QStringLiteral("Invalid state transition"));
        return;
    }
}

void MobileConnectServer::handleDisconnected()
{
    timeoutTimer_->stop();
    readBuffer_.clear();
    pendingCode_.clear();
    connectionState_ = ConnectionState::Closed;

    if (activeSocket_ == nullptr) {
        return;
    }

    activeSocket_->deleteLater();
    activeSocket_ = nullptr;
}

void MobileConnectServer::handleTimeout()
{
    if (connectionState_ == ConnectionState::WaitHello) {
        sendErrorAndClose(QStringLiteral("Hello timeout"));
        return;
    }

    if (connectionState_ == ConnectionState::WaitPayload) {
        sendErrorAndClose(QStringLiteral("Payload timeout"));
    }
}

bool MobileConnectServer::processHelloMessage(const QJsonObject &message)
{
    QString code;
    QString errorMessage;
    if (!validateHelloMessage(message, &code, &errorMessage)) {
        sendErrorAndClose(errorMessage);
        return false;
    }

    timeoutTimer_->stop();
    pendingCode_ = code;

    const auto accepted = confirmationHandler_ != nullptr && confirmationHandler_(formattedCode(code));
    if (!accepted) {
        sendJsonMessageAndClose(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("continue")},
            {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("Transfer rejected by user")},
        });
        return false;
    }

    sendJsonMessage(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("continue")},
        {QStringLiteral("ok"), true},
    });
    connectionState_ = ConnectionState::WaitPayload;
    timeoutTimer_->start(payloadTimeoutMs_);
    return true;
}

bool MobileConnectServer::processPayloadMessage(const QJsonObject &message)
{
    if (message.value(QStringLiteral("type")).toString() != "ideas"_L1) {
        sendErrorAndClose(QStringLiteral("Unexpected message type"));
        return false;
    }
    if (message.value(QStringLiteral("version")).toInt() != 1) {
        sendErrorAndClose(QStringLiteral("Unsupported version"));
        return false;
    }
    if (!message.value(QStringLiteral("items")).isArray()) {
        sendErrorAndClose(QStringLiteral("Ideas payload must contain an items array"));
        return false;
    }

    const auto items = message.value(QStringLiteral("items")).toArray();
    if (QJsonDocument{items}.toJson(QJsonDocument::Compact).size() > maxMessageSizeBytes_) {
        sendErrorAndClose(QStringLiteral("Payload too large"));
        return false;
    }

    int importedCount = 0;
    QString errorMessage;
    if (controller_ == nullptr
        || !controller_->importTransferredIdeas(items.toVariantList(), &importedCount, &errorMessage)) {
        sendJsonMessageAndClose(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("result")},
            {QStringLiteral("ok"), false},
            {QStringLiteral("message"), errorMessage.isEmpty() ? QStringLiteral("No valid ideas found") : errorMessage},
        });
        return false;
    }

    sendJsonMessageAndClose(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("result")},
        {QStringLiteral("ok"), true},
        {QStringLiteral("imported"), importedCount},
    });
    return true;
}

void MobileConnectServer::sendErrorAndClose(const QString &message)
{
    sendJsonMessageAndClose(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("error")},
        {QStringLiteral("message"), message},
    });
}

void MobileConnectServer::sendJsonMessage(const QJsonObject &message)
{
    if (activeSocket_ == nullptr) {
        return;
    }

    const auto payload = QJsonDocument{message}.toJson(QJsonDocument::Compact) + '\n';
    activeSocket_->write(payload);
    activeSocket_->flush();
}

void MobileConnectServer::sendJsonMessageAndClose(const QJsonObject &message)
{
    sendJsonMessage(message);
    if (activeSocket_ != nullptr) {
        activeSocket_->waitForBytesWritten(1000);
        activeSocket_->disconnectFromHost();
    }
}

bool MobileConnectServer::validateHelloMessage(const QJsonObject &message,
                                               QString *code,
                                               QString *errorMessage) const
{
    if (message.value(QStringLiteral("type")).toString() != "hello"_L1) {
        *errorMessage = QStringLiteral("Expected hello message");
        return false;
    }
    if (message.value(QStringLiteral("app")).toString() != "smtool-transfer"_L1) {
        *errorMessage = QStringLiteral("Unsupported app identifier");
        return false;
    }
    if (message.value(QStringLiteral("version")).toInt() != 1) {
        *errorMessage = QStringLiteral("Unsupported version");
        return false;
    }

    const auto parsedCode = message.value(QStringLiteral("code")).toString().trimmed();
    static const QRegularExpression codePattern(QStringLiteral(R"(^\d{8}$)"));
    if (!codePattern.match(parsedCode).hasMatch()) {
        *errorMessage = QStringLiteral("Confirmation code must contain exactly eight digits");
        return false;
    }

    *code = parsedCode;
    return true;
}

QString MobileConnectServer::normalizeListenIp(const QString &listenIp) const
{
    const auto trimmed = listenIp.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("0.0.0.0") : trimmed;
}

} // namespace SmTool::App
