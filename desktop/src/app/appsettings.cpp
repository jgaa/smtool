#include "app/appsettings.h"

#include "app/loggingcontroller.h"
#include "data/database.h"

#include <QDir>
#include <QSettings>

#include <algorithm>

namespace SmTool::App {

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    ensureDefaults();
}

QString AppSettings::configuredDatabasePath() const
{
    return normalizedPath(QSettings{}.value(QStringLiteral("system/databasePath")).toString());
}

void AppSettings::setConfiguredDatabasePath(const QString &path)
{
    const auto normalized = normalizedPath(path);
    if (configuredDatabasePath() == normalized) {
        return;
    }

    saveValue(QStringLiteral("system/databasePath"), normalized);
    emit configuredDatabasePathChanged();
}

QString AppSettings::defaultDatabasePath() const
{
    return Data::Database::defaultDatabasePath();
}

int AppSettings::boardDescriptionPreviewWordCap() const
{
    return normalizedBoardDescriptionPreviewWordCap(
        QSettings{}.value(QStringLiteral("ui/boardDescriptionPreviewWordCap"), 10).toInt());
}

void AppSettings::setBoardDescriptionPreviewWordCap(int value)
{
    const auto normalized = normalizedBoardDescriptionPreviewWordCap(value);
    if (boardDescriptionPreviewWordCap() == normalized) {
        return;
    }

    saveValue(QStringLiteral("ui/boardDescriptionPreviewWordCap"), normalized);
    emit boardDescriptionPreviewWordCapChanged();
}

QString AppSettings::effectiveDatabasePath(const QString &overridePath) const
{
    const auto normalizedOverride = normalizedPath(overridePath);
    if (!normalizedOverride.isEmpty()) {
        return normalizedOverride;
    }

    const auto configured = configuredDatabasePath();
    return configured.isEmpty() ? defaultDatabasePath() : configured;
}

int AppSettings::appLogLevel() const
{
    return QSettings{}.value(QStringLiteral("logging/applevel"), LoggingController::InfoLevel).toInt();
}

void AppSettings::setAppLogLevel(int level)
{
    if (appLogLevel() == level) {
        return;
    }

    saveValue(QStringLiteral("logging/applevel"), level);
    emit appLogLevelChanged();
}

int AppSettings::fileLogLevel() const
{
    return QSettings{}.value(QStringLiteral("logging/level"), LoggingController::DisabledLevel).toInt();
}

void AppSettings::setFileLogLevel(int level)
{
    if (fileLogLevel() == level) {
        return;
    }

    saveValue(QStringLiteral("logging/level"), level);
    emit fileLogLevelChanged();
}

QString AppSettings::logFilePath() const
{
    return normalizedPath(QSettings{}.value(QStringLiteral("logging/path")).toString());
}

void AppSettings::setLogFilePath(const QString &path)
{
    const auto normalized = normalizedPath(path);
    if (logFilePath() == normalized) {
        return;
    }

    saveValue(QStringLiteral("logging/path"), normalized);
    emit logFilePathChanged();
}

bool AppSettings::pruneLogFile() const
{
    return QSettings{}.value(QStringLiteral("logging/prune"), false).toBool();
}

void AppSettings::setPruneLogFile(bool enabled)
{
    if (pruneLogFile() == enabled) {
        return;
    }

    saveValue(QStringLiteral("logging/prune"), enabled);
    emit pruneLogFileChanged();
}

bool AppSettings::confirmContentDeletion() const
{
    return QSettings{}.value(QStringLiteral("ui/confirmContentDeletion"), true).toBool();
}

void AppSettings::setConfirmContentDeletion(bool enabled)
{
    if (confirmContentDeletion() == enabled) {
        return;
    }

    saveValue(QStringLiteral("ui/confirmContentDeletion"), enabled);
    emit confirmContentDeletionChanged();
}

void AppSettings::ensureDefaults() const
{
    QSettings settings;
    LoggingController{}.ensureDefaults(settings);
    if (!settings.contains(QStringLiteral("system/databasePath"))) {
        settings.setValue(QStringLiteral("system/databasePath"), QString{});
    }
    if (!settings.contains(QStringLiteral("ui/boardDescriptionPreviewWordCap"))) {
        settings.setValue(QStringLiteral("ui/boardDescriptionPreviewWordCap"), 10);
    }
    if (!settings.contains(QStringLiteral("ui/confirmContentDeletion"))) {
        settings.setValue(QStringLiteral("ui/confirmContentDeletion"), true);
    }
    settings.sync();
}

void AppSettings::saveValue(const QString &key, const QVariant &value) const
{
    QSettings settings;
    settings.setValue(key, value);
    settings.sync();
}

QString AppSettings::normalizedPath(const QString &path) const
{
    const auto trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    return QDir::cleanPath(trimmed);
}

int AppSettings::normalizedBoardDescriptionPreviewWordCap(int value) const
{
    return std::clamp(value, 3, 30);
}

} // namespace SmTool::App
