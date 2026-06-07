#include "app/appsettings.h"

#include "app/loggingcontroller.h"
#include "data/database.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

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

QString AppSettings::configuredMediaDataDir() const
{
    return normalizedPath(QSettings{}.value(QStringLiteral("system/mediaDataDir")).toString());
}

void AppSettings::setConfiguredMediaDataDir(const QString &path)
{
    const auto normalized = normalizedPath(path);
    if (configuredMediaDataDir() == normalized) {
        return;
    }

    saveValue(QStringLiteral("system/mediaDataDir"), normalized);
    emit configuredMediaDataDirChanged();
}

QString AppSettings::defaultMediaDataDir() const
{
    return normalizedPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

QString AppSettings::effectiveMediaDataDir() const
{
    const auto configured = configuredMediaDataDir();
    return configured.isEmpty() ? defaultMediaDataDir() : configured;
}

int AppSettings::defaultContentPriority() const
{
    return normalizedDefaultContentPriority(
        QSettings{}.value(QStringLiteral("ui/defaultContentPriority"), 5).toInt());
}

void AppSettings::setDefaultContentPriority(int value)
{
    const auto normalized = normalizedDefaultContentPriority(value);
    if (defaultContentPriority() == normalized) {
        return;
    }

    saveValue(QStringLiteral("ui/defaultContentPriority"), normalized);
    emit defaultContentPriorityChanged();
}

bool AppSettings::batchMarkdownImports() const
{
    return QSettings{}.value(QStringLiteral("ui/batchMarkdownImports"), true).toBool();
}

void AppSettings::setBatchMarkdownImports(bool enabled)
{
    if (batchMarkdownImports() == enabled) {
        return;
    }

    saveValue(QStringLiteral("ui/batchMarkdownImports"), enabled);
    emit batchMarkdownImportsChanged();
}

int AppSettings::importedIdeaTitleWordCap() const
{
    return normalizedImportedIdeaTitleWordCap(
        QSettings{}.value(QStringLiteral("ui/importedIdeaTitleWordCap"), 8).toInt());
}

void AppSettings::setImportedIdeaTitleWordCap(int value)
{
    const auto normalized = normalizedImportedIdeaTitleWordCap(value);
    if (importedIdeaTitleWordCap() == normalized) {
        return;
    }

    saveValue(QStringLiteral("ui/importedIdeaTitleWordCap"), normalized);
    emit importedIdeaTitleWordCapChanged();
}

int AppSettings::cardDescriptionWordCap() const
{
    return normalizedCardDescriptionWordCap(
        QSettings{}.value(QStringLiteral("ui/cardDescriptionWordCap"), 100).toInt());
}

void AppSettings::setCardDescriptionWordCap(int value)
{
    const auto normalized = normalizedCardDescriptionWordCap(value);
    if (cardDescriptionWordCap() == normalized) {
        return;
    }

    saveValue(QStringLiteral("ui/cardDescriptionWordCap"), normalized);
    emit cardDescriptionWordCapChanged();
}

bool AppSettings::cardDescriptionMarkdownEnabled() const
{
    return QSettings{}.value(QStringLiteral("ui/cardDescriptionMarkdownEnabled"), true).toBool();
}

void AppSettings::setCardDescriptionMarkdownEnabled(bool enabled)
{
    if (cardDescriptionMarkdownEnabled() == enabled) {
        return;
    }

    saveValue(QStringLiteral("ui/cardDescriptionMarkdownEnabled"), enabled);
    emit cardDescriptionMarkdownEnabledChanged();
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

bool AppSettings::fetchAddedUrlTitles() const
{
    return QSettings{}.value(QStringLiteral("ui/fetchAddedUrlTitles"), false).toBool();
}

void AppSettings::setFetchAddedUrlTitles(bool enabled)
{
    if (fetchAddedUrlTitles() == enabled) {
        return;
    }

    saveValue(QStringLiteral("ui/fetchAddedUrlTitles"), enabled);
    emit fetchAddedUrlTitlesChanged();
}

void AppSettings::ensureDefaults() const
{
    QSettings settings;
    LoggingController{}.ensureDefaults(settings);
    if (!settings.contains(QStringLiteral("system/databasePath"))) {
        settings.setValue(QStringLiteral("system/databasePath"), QString{});
    }
    if (!settings.contains(QStringLiteral("system/mediaDataDir"))) {
        settings.setValue(QStringLiteral("system/mediaDataDir"), QString{});
    }
    if (!settings.contains(QStringLiteral("ui/defaultContentPriority"))) {
        settings.setValue(QStringLiteral("ui/defaultContentPriority"), 5);
    }
    if (!settings.contains(QStringLiteral("ui/batchMarkdownImports"))) {
        settings.setValue(QStringLiteral("ui/batchMarkdownImports"), true);
    }
    if (!settings.contains(QStringLiteral("ui/importedIdeaTitleWordCap"))) {
        settings.setValue(QStringLiteral("ui/importedIdeaTitleWordCap"), 8);
    }
    if (!settings.contains(QStringLiteral("ui/cardDescriptionWordCap"))) {
        settings.setValue(QStringLiteral("ui/cardDescriptionWordCap"), 100);
    }
    if (!settings.contains(QStringLiteral("ui/cardDescriptionMarkdownEnabled"))) {
        settings.setValue(QStringLiteral("ui/cardDescriptionMarkdownEnabled"), true);
    }
    if (!settings.contains(QStringLiteral("ui/confirmContentDeletion"))) {
        settings.setValue(QStringLiteral("ui/confirmContentDeletion"), true);
    }
    if (!settings.contains(QStringLiteral("ui/fetchAddedUrlTitles"))) {
        settings.setValue(QStringLiteral("ui/fetchAddedUrlTitles"), false);
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

int AppSettings::normalizedDefaultContentPriority(int value) const
{
    return std::clamp(value, 0, 100);
}

int AppSettings::normalizedImportedIdeaTitleWordCap(int value) const
{
    return std::clamp(value, 1, 30);
}

int AppSettings::normalizedCardDescriptionWordCap(int value) const
{
    return std::clamp(value, 0, 500);
}

} // namespace SmTool::App
