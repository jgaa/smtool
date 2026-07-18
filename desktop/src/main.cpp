#include "app/appcontroller.h"
#include "app/appinfo.h"
#include "app/mobileconnectserver.h"
#include "app/appsettings.h"
#include "app/loggingcontroller.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QIcon>
#include <QQmlError>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SmTool"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SMTOOL_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("smtool"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/smtool-talk.svg")));

    SmTool::App::AppInfo appInfo;
    SmTool::App::AppSettings appSettings;
    SmTool::App::LoggingController loggingController;
    loggingController.initialize();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Local-first content strategy POC"));
    parser.addHelpOption();

    QCommandLineOption seedDemoDataOption{
        QStringLiteral("seed-demo-data"),
        QStringLiteral("Insert demo content into an empty database."),
    };
    QCommandLineOption databasePathOption{
        QStringLiteral("database-path"),
        QStringLiteral("Override the SQLite database file path."),
        QStringLiteral("path"),
    };
    parser.addOption(seedDemoDataOption);
    parser.addOption(databasePathOption);
    parser.process(app);

    const auto commandLineDatabasePath = parser.value(databasePathOption).trimmed();
    const auto effectiveDatabasePath = appSettings.effectiveDatabasePath(commandLineDatabasePath);
    const auto databaseDirectory = QFileInfo{effectiveDatabasePath}.absolutePath();
    if (!QDir{}.mkpath(databaseDirectory)) {
        const auto message = QStringLiteral("SmTool could not create the database directory:\n%1").arg(databaseDirectory);
        LOG_ERROR << message.toStdString();
        QMessageBox::critical(nullptr, QStringLiteral("SmTool"), message);
        return 1;
    }

    QLockFile appLock{QDir{databaseDirectory}.filePath(QStringLiteral("smtool.lock"))};
    if (!appLock.tryLock()) {
        const auto message = QStringLiteral(
            "SmTool is already running for this database location.\n\n"
            "Close the other instance before starting a new one.");
        LOG_ERROR << "Failed to acquire app lock at '"
                  << appLock.fileName().toStdString()
                  << "': another instance is already running";
        QMessageBox::warning(nullptr, QStringLiteral("SmTool Already Running"), message);
        return 1;
    }

    if (commandLineDatabasePath.isEmpty() && effectiveDatabasePath != appSettings.defaultDatabasePath()) {
        QString startupMoveError;
        if (!SmTool::Data::Database::moveDatabaseFile(appSettings.defaultDatabasePath(), effectiveDatabasePath, &startupMoveError)) {
            LOG_ERROR << "Failed to move database during startup: " << startupMoveError.toStdString();
        }
    }

    SmTool::Data::Database::Options databaseOptions;
    databaseOptions.seedDemoData = parser.isSet(seedDemoDataOption);
    databaseOptions.databaseFilePath = effectiveDatabasePath;

    SmTool::App::AppController controller{databaseOptions};
    QString errorMessage;
    if (!controller.initialize(&errorMessage)) {
        LOG_ERROR << "Failed to initialize SmTool: " << errorMessage.toStdString();
        return 1;
    }
    controller.setDefaultContentPriority(appSettings.defaultContentPriority());
    QObject::connect(&appSettings, &SmTool::App::AppSettings::defaultContentPriorityChanged,
                     &controller, [&appSettings, &controller]() {
        controller.setDefaultContentPriority(appSettings.defaultContentPriority());
    });
    controller.setBatchMarkdownImportsEnabled(appSettings.batchMarkdownImports());
    QObject::connect(&appSettings, &SmTool::App::AppSettings::batchMarkdownImportsChanged,
                     &controller, [&appSettings, &controller]() {
        controller.setBatchMarkdownImportsEnabled(appSettings.batchMarkdownImports());
    });
    controller.setImportedIdeaTitleWordCap(appSettings.importedIdeaTitleWordCap());
    QObject::connect(&appSettings, &SmTool::App::AppSettings::importedIdeaTitleWordCapChanged,
                     &controller, [&appSettings, &controller]() {
        controller.setImportedIdeaTitleWordCap(appSettings.importedIdeaTitleWordCap());
    });
    controller.setDescriptionPreviewWordCap(appSettings.cardDescriptionWordCap());
    QObject::connect(&appSettings, &SmTool::App::AppSettings::cardDescriptionWordCapChanged,
                     &controller, [&appSettings, &controller]() {
        controller.setDescriptionPreviewWordCap(appSettings.cardDescriptionWordCap());
    });

    SmTool::App::MobileConnectServer mobileConnectServer{&controller};
    mobileConnectServer.setConfirmationHandler([](const QString &formattedCode) {
        const auto button = QMessageBox::question(
            QApplication::activeWindow(),
            QStringLiteral("Transfer Request"),
            QStringLiteral("Transfer request received\n\nCode: %1\n\nAccept transfer?").arg(formattedCode),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        return button == QMessageBox::Yes;
    });
    mobileConnectServer.applySettings(appSettings.mobileConnectEnabled(),
                                      appSettings.mobileConnectListenIp(),
                                      static_cast<quint16>(appSettings.mobileConnectPort()));
    QObject::connect(&appSettings, &SmTool::App::AppSettings::mobileConnectEnabledChanged,
                     &mobileConnectServer, [&appSettings, &mobileConnectServer]() {
        mobileConnectServer.setEnabled(appSettings.mobileConnectEnabled());
    });
    QObject::connect(&appSettings, &SmTool::App::AppSettings::mobileConnectListenIpChanged,
                     &mobileConnectServer, [&appSettings, &mobileConnectServer]() {
        mobileConnectServer.setListenIp(appSettings.mobileConnectListenIp());
    });
    QObject::connect(&appSettings, &SmTool::App::AppSettings::mobileConnectPortChanged,
                     &mobileConnectServer, [&appSettings, &mobileConnectServer]() {
        mobileConnectServer.setPort(static_cast<quint16>(appSettings.mobileConnectPort()));
    });

    LOG_INFO << "Starting SmTool";
    LOG_INFO << "Configuration from '" << loggingController.settingsFilePath().toStdString() << "'";

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) {
            LOG_ERROR << "QML: " << warning.toString().toStdString();
        }
    });
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("appInfo"), &appInfo);
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/SmTool/src/ui/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        LOG_ERROR << "Failed to create a QML root object.";
        return 1;
    }

    return app.exec();
}
