#include "app/appcontroller.h"
#include "app/appinfo.h"
#include "app/appsettings.h"
#include "app/loggingcontroller.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QQmlError>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SmTool"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SMTOOL_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("smtool"));

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
    controller.setDescriptionPreviewWordCap(appSettings.boardDescriptionPreviewWordCap());
    QObject::connect(&appSettings, &SmTool::App::AppSettings::boardDescriptionPreviewWordCapChanged,
                     &controller, [&appSettings, &controller]() {
        controller.setDescriptionPreviewWordCap(appSettings.boardDescriptionPreviewWordCap());
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
