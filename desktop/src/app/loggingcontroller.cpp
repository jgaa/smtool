#include "app/loggingcontroller.h"

#include <QFileInfo>

#include <iostream>
#include <memory>

namespace SmTool::App {

void LoggingController::ensureDefaults(QSettings &settings) const
{
    if (!settings.contains(QStringLiteral("logging/applevel"))) {
        settings.setValue(QStringLiteral("logging/applevel"), InfoLevel);
    }
    if (!settings.contains(QStringLiteral("logging/level"))) {
        settings.setValue(QStringLiteral("logging/level"), DisabledLevel);
    }
    if (!settings.contains(QStringLiteral("logging/path"))) {
        settings.setValue(QStringLiteral("logging/path"), QString{});
    }
    if (!settings.contains(QStringLiteral("logging/prune"))) {
        settings.setValue(QStringLiteral("logging/prune"), false);
    }
}

void LoggingController::initialize() const
{
    QSettings settings;
    ensureDefaults(settings);
    settings.sync();

#ifdef Q_OS_LINUX
    if (const auto level = settings.value(QStringLiteral("logging/applevel"), InfoLevel).toInt();
        level > DisabledLevel) {
        logfault::LogManager::Instance().AddHandler(
            std::make_unique<logfault::StreamHandler>(std::clog, static_cast<logfault::LogLevel>(level)));
    }
#endif

    if (const auto level = settings.value(QStringLiteral("logging/level"), DisabledLevel).toInt();
        level > DisabledLevel) {
        const auto path = settings.value(QStringLiteral("logging/path")).toString().trimmed();
        if (!path.isEmpty()) {
            const auto prune = settings.value(QStringLiteral("logging/prune"), false).toBool();
            logfault::LogManager::Instance().AddHandler(
                std::make_unique<logfault::StreamHandler>(path.toStdString(),
                                                          static_cast<logfault::LogLevel>(level),
                                                          prune));
        }
    }
}

QString LoggingController::settingsFilePath() const
{
    return QFileInfo(QSettings{}.fileName()).absoluteFilePath();
}

} // namespace SmTool::App
