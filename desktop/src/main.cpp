#include "app/appcontroller.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlError>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SmTool"));
    QCoreApplication::setOrganizationName(QStringLiteral("smtool"));

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

    SmTool::Data::Database::Options databaseOptions;
    databaseOptions.seedDemoData = parser.isSet(seedDemoDataOption);
    databaseOptions.databaseFilePath = parser.value(databasePathOption);

    SmTool::App::AppController controller{databaseOptions};
    QString errorMessage;
    if (!controller.initialize(&errorMessage)) {
        qCritical().noquote() << "Failed to initialize SmTool:" << errorMessage;
        return 1;
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) {
            qCritical().noquote() << warning.toString();
        }
    });
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/SmTool/src/ui/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical().noquote() << "Failed to create a QML root object.";
        return 1;
    }

    return app.exec();
}
