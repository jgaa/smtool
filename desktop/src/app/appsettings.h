#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace SmTool::App {

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString configuredDatabasePath READ configuredDatabasePath WRITE setConfiguredDatabasePath NOTIFY configuredDatabasePathChanged)
    Q_PROPERTY(QString defaultDatabasePath READ defaultDatabasePath CONSTANT)
    Q_PROPERTY(QString configuredMediaDataDir READ configuredMediaDataDir WRITE setConfiguredMediaDataDir NOTIFY configuredMediaDataDirChanged)
    Q_PROPERTY(QString defaultMediaDataDir READ defaultMediaDataDir CONSTANT)
    Q_PROPERTY(QString effectiveMediaDataDir READ effectiveMediaDataDir NOTIFY configuredMediaDataDirChanged)
    Q_PROPERTY(int defaultContentPriority READ defaultContentPriority WRITE setDefaultContentPriority NOTIFY defaultContentPriorityChanged)
    Q_PROPERTY(bool batchMarkdownImports READ batchMarkdownImports WRITE setBatchMarkdownImports NOTIFY batchMarkdownImportsChanged)
    Q_PROPERTY(int importedIdeaTitleWordCap READ importedIdeaTitleWordCap WRITE setImportedIdeaTitleWordCap NOTIFY importedIdeaTitleWordCapChanged)
    Q_PROPERTY(int cardDescriptionWordCap READ cardDescriptionWordCap WRITE setCardDescriptionWordCap NOTIFY cardDescriptionWordCapChanged)
    Q_PROPERTY(bool cardDescriptionMarkdownEnabled READ cardDescriptionMarkdownEnabled WRITE setCardDescriptionMarkdownEnabled NOTIFY cardDescriptionMarkdownEnabledChanged)
    Q_PROPERTY(int appLogLevel READ appLogLevel WRITE setAppLogLevel NOTIFY appLogLevelChanged)
    Q_PROPERTY(int fileLogLevel READ fileLogLevel WRITE setFileLogLevel NOTIFY fileLogLevelChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath WRITE setLogFilePath NOTIFY logFilePathChanged)
    Q_PROPERTY(bool pruneLogFile READ pruneLogFile WRITE setPruneLogFile NOTIFY pruneLogFileChanged)
    Q_PROPERTY(bool confirmContentDeletion READ confirmContentDeletion WRITE setConfirmContentDeletion NOTIFY confirmContentDeletionChanged)
    Q_PROPERTY(bool fetchAddedUrlTitles READ fetchAddedUrlTitles WRITE setFetchAddedUrlTitles NOTIFY fetchAddedUrlTitlesChanged)
    Q_PROPERTY(int calendarFirstDayOfWeek READ calendarFirstDayOfWeek WRITE setCalendarFirstDayOfWeek NOTIFY calendarFirstDayOfWeekChanged)
    Q_PROPERTY(int localeFirstDayOfWeek READ localeFirstDayOfWeek CONSTANT)
    Q_PROPERTY(bool mobileConnectEnabled READ mobileConnectEnabled WRITE setMobileConnectEnabled NOTIFY mobileConnectEnabledChanged)
    Q_PROPERTY(QString mobileConnectListenIp READ mobileConnectListenIp WRITE setMobileConnectListenIp NOTIFY mobileConnectListenIpChanged)
    Q_PROPERTY(int mobileConnectPort READ mobileConnectPort WRITE setMobileConnectPort NOTIFY mobileConnectPortChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    [[nodiscard]] QString configuredDatabasePath() const;
    void setConfiguredDatabasePath(const QString &path);

    [[nodiscard]] QString defaultDatabasePath() const;
    [[nodiscard]] QString effectiveDatabasePath(const QString &overridePath = {}) const;
    [[nodiscard]] QString configuredMediaDataDir() const;
    void setConfiguredMediaDataDir(const QString &path);
    [[nodiscard]] QString defaultMediaDataDir() const;
    [[nodiscard]] QString effectiveMediaDataDir() const;

    [[nodiscard]] int defaultContentPriority() const;
    void setDefaultContentPriority(int value);

    [[nodiscard]] bool batchMarkdownImports() const;
    void setBatchMarkdownImports(bool enabled);

    [[nodiscard]] int importedIdeaTitleWordCap() const;
    void setImportedIdeaTitleWordCap(int value);

    [[nodiscard]] int cardDescriptionWordCap() const;
    void setCardDescriptionWordCap(int value);

    [[nodiscard]] bool cardDescriptionMarkdownEnabled() const;
    void setCardDescriptionMarkdownEnabled(bool enabled);

    [[nodiscard]] int appLogLevel() const;
    void setAppLogLevel(int level);

    [[nodiscard]] int fileLogLevel() const;
    void setFileLogLevel(int level);

    [[nodiscard]] QString logFilePath() const;
    void setLogFilePath(const QString &path);

    [[nodiscard]] bool pruneLogFile() const;
    void setPruneLogFile(bool enabled);

    [[nodiscard]] bool confirmContentDeletion() const;
    void setConfirmContentDeletion(bool enabled);

    [[nodiscard]] bool fetchAddedUrlTitles() const;
    void setFetchAddedUrlTitles(bool enabled);
    [[nodiscard]] int calendarFirstDayOfWeek() const;
    void setCalendarFirstDayOfWeek(int day);
    [[nodiscard]] int localeFirstDayOfWeek() const;

    [[nodiscard]] bool mobileConnectEnabled() const;
    void setMobileConnectEnabled(bool enabled);

    [[nodiscard]] QString mobileConnectListenIp() const;
    void setMobileConnectListenIp(const QString &ipAddress);

    [[nodiscard]] int mobileConnectPort() const;
    void setMobileConnectPort(int port);

    void ensureDefaults() const;

signals:
    void configuredDatabasePathChanged();
    void configuredMediaDataDirChanged();
    void defaultContentPriorityChanged();
    void batchMarkdownImportsChanged();
    void importedIdeaTitleWordCapChanged();
    void cardDescriptionWordCapChanged();
    void cardDescriptionMarkdownEnabledChanged();
    void appLogLevelChanged();
    void fileLogLevelChanged();
    void logFilePathChanged();
    void pruneLogFileChanged();
    void confirmContentDeletionChanged();
    void fetchAddedUrlTitlesChanged();
    void calendarFirstDayOfWeekChanged();
    void mobileConnectEnabledChanged();
    void mobileConnectListenIpChanged();
    void mobileConnectPortChanged();

private:
    void saveValue(const QString &key, const QVariant &value) const;
    [[nodiscard]] QString normalizedPath(const QString &path) const;
    [[nodiscard]] QString normalizedMobileConnectListenIp(const QString &ipAddress) const;
    [[nodiscard]] int normalizedDefaultContentPriority(int value) const;
    [[nodiscard]] int normalizedImportedIdeaTitleWordCap(int value) const;
    [[nodiscard]] int normalizedCardDescriptionWordCap(int value) const;
    [[nodiscard]] int normalizedMobileConnectPort(int value) const;
    [[nodiscard]] int normalizedCalendarFirstDayOfWeek(int value) const;
};

} // namespace SmTool::App
