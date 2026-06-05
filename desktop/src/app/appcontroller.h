#pragma once

#include "data/contentrepository.h"
#include "data/dashboardrepository.h"
#include "data/database.h"
#include "data/lookupsrepository.h"
#include "data/mediarepository.h"
#include "data/publicationrepository.h"
#include "data/seriesrepository.h"
#include "models/calendarentrymodel.h"
#include "models/contentlistmodel.h"
#include "models/contentstatuslistmodel.h"
#include "models/dashboardmetricmodel.h"
#include "models/lookuplistmodel.h"
#include "models/serieslistmodel.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <map>
#include <memory>

namespace SmTool::App {

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SmTool::Models::ContentListModel *inboxModel READ inboxModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentStatusListModel *contentStatusModel READ contentStatusModel CONSTANT)
    Q_PROPERTY(SmTool::Models::CalendarEntryModel *calendarModel READ calendarModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *allContentModel READ allContentModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *sourceModel READ sourceModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *derivativeModel READ derivativeModel CONSTANT)
    Q_PROPERTY(SmTool::Models::SeriesListModel *seriesModel READ seriesModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *pillarModel READ pillarModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *kindModel READ kindModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *channelModel READ channelModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardByPillarModel READ dashboardByPillarModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardBySeriesModel READ dashboardBySeriesModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardByStatusModel READ dashboardByStatusModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardUpcomingModel READ dashboardUpcomingModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardPublishedContentModel READ dashboardPublishedContentModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardPublishedPublicationsModel READ dashboardPublishedPublicationsModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardMetricModel *dashboardZeroPublishedPillarsModel READ dashboardZeroPublishedPillarsModel CONSTANT)
    Q_PROPERTY(bool boardShowArchived READ boardShowArchived WRITE setBoardShowArchived NOTIFY boardShowArchivedChanged)
    Q_PROPERTY(bool allContentShowArchived READ allContentShowArchived WRITE setAllContentShowArchived NOTIFY allContentShowArchivedChanged)
    Q_PROPERTY(bool dashboardIncludeArchived READ dashboardIncludeArchived WRITE setDashboardIncludeArchived NOTIFY dashboardIncludeArchivedChanged)
    Q_PROPERTY(bool calendarIncludeArchived READ calendarIncludeArchived WRITE setCalendarIncludeArchived NOTIFY calendarIncludeArchivedChanged)
    Q_PROPERTY(bool calendarIncludePublished READ calendarIncludePublished WRITE setCalendarIncludePublished NOTIFY calendarIncludePublishedChanged)
    Q_PROPERTY(bool clipboardHasText READ clipboardHasText NOTIFY clipboardHasTextChanged)
    Q_PROPERTY(QString currentSourceId READ currentSourceId WRITE setCurrentSourceId NOTIFY currentSourceIdChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit AppController(Data::Database::Options databaseOptions = {}, QObject *parent = nullptr);

    bool initialize(QString *errorMessage = nullptr);
    void setDescriptionPreviewWordCap(int value);

    [[nodiscard]] Models::ContentListModel *inboxModel();
    [[nodiscard]] Models::ContentStatusListModel *contentStatusModel();
    [[nodiscard]] Models::CalendarEntryModel *calendarModel();
    [[nodiscard]] Models::ContentListModel *allContentModel();
    [[nodiscard]] Models::ContentListModel *sourceModel();
    [[nodiscard]] Models::ContentListModel *derivativeModel();
    [[nodiscard]] Models::SeriesListModel *seriesModel();
    [[nodiscard]] Models::LookupListModel *pillarModel();
    [[nodiscard]] Models::LookupListModel *kindModel();
    [[nodiscard]] Models::LookupListModel *channelModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardByPillarModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardBySeriesModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardByStatusModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardUpcomingModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardPublishedContentModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardPublishedPublicationsModel();
    [[nodiscard]] Models::DashboardMetricModel *dashboardZeroPublishedPillarsModel();

    [[nodiscard]] bool boardShowArchived() const;
    void setBoardShowArchived(bool enabled);

    [[nodiscard]] bool allContentShowArchived() const;
    void setAllContentShowArchived(bool enabled);

    [[nodiscard]] bool dashboardIncludeArchived() const;
    void setDashboardIncludeArchived(bool enabled);

    [[nodiscard]] bool calendarIncludeArchived() const;
    void setCalendarIncludeArchived(bool enabled);

    [[nodiscard]] bool calendarIncludePublished() const;
    void setCalendarIncludePublished(bool enabled);

    [[nodiscard]] bool clipboardHasText() const;

    [[nodiscard]] QString currentSourceId() const;
    void setCurrentSourceId(const QString &id);

    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString &value);

    [[nodiscard]] QString statusMessage() const;
    Q_INVOKABLE int allContentSortMode() const;
    Q_INVOKABLE void setAllContentSortMode(int mode);
    Q_INVOKABLE void clearSearchQuery();

    Q_INVOKABLE bool refreshAll();
    Q_INVOKABLE bool applyDatabasePath(const QString &path);
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE bool pasteClipboardToIdea();
    Q_INVOKABLE bool createIdeaFromText(const QString &text);
    Q_INVOKABLE bool importIdeasFromUserSelectedFile();
    Q_INVOKABLE bool importIdeasFromFile(const QString &filePath);
    Q_INVOKABLE QString chooseMediaFile() const;
    Q_INVOKABLE QString localPathFromUrl(const QString &urlText) const;
    Q_INVOKABLE bool openMedia(const QVariantMap &mediaItem, const QString &mediaDataDir) const;
    Q_INVOKABLE QString copyMediaFileToDataDir(const QString &sourcePath, const QString &mediaDataDir);
    Q_INVOKABLE void logDebug(const QString &message) const;
    Q_INVOKABLE QObject *boardModelForStatus(const QString &statusId) const;
    Q_INVOKABLE QVariantMap contentDetails(const QString &contentId) const;
    Q_INVOKABLE QVariantMap publicationDetails(const QString &publicationId) const;
    Q_INVOKABLE bool updateContent(const QString &contentId,
                                   const QString &title,
                                   const QString &description,
                                   const QString &tags,
                                   const QString &pillarId,
                                   int priority,
                                   const QString &scheduledAt,
                                   const QString &suggestedChannelId,
                                   const QString &status,
                                   const QVariantList &mediaItems,
                                   const QString &mediaDataDir,
                                   bool fetchUrlTitles);
    Q_INVOKABLE bool savePublication(const QString &contentId,
                                     const QString &publicationId,
                                     const QString &channelId,
                                     const QString &status,
                                     const QString &scheduledAt,
                                     const QString &publishedAt,
                                     const QString &url,
                                     const QVariantList &mediaItems,
                                     const QString &mediaDataDir,
                                     bool fetchUrlTitles);
    Q_INVOKABLE bool deletePublication(const QString &publicationId);
    Q_INVOKABLE bool deleteContent(const QString &contentId);
    Q_INVOKABLE bool createInboxItem(const QString &title,
                                     const QString &description,
                                     const QString &tags,
                                     const QString &pillarId,
                                     const QString &seriesId,
                                     int priority,
                                     const QString &scheduledAt,
                                     const QString &suggestedChannelId,
                                     const QVariantList &mediaItems = {},
                                     const QString &mediaDataDir = {},
                                     bool fetchUrlTitles = false);
    Q_INVOKABLE bool moveContentToStatus(const QString &contentId, const QString &targetStatus);
    Q_INVOKABLE bool createSeries(const QString &name, const QString &description, const QString &pillarId);
    Q_INVOKABLE QVariantList burstTemplateOptions() const;
    Q_INVOKABLE bool createBurstForCurrentSource(const QVariantList &templateKeys);

signals:
    void boardShowArchivedChanged();
    void allContentShowArchivedChanged();
    void dashboardIncludeArchivedChanged();
    void calendarIncludeArchivedChanged();
    void calendarIncludePublishedChanged();
    void clipboardHasTextChanged();
    void currentSourceIdChanged();
    void searchQueryChanged();
    void statusMessageChanged();

private:
    void initializeRepositories();
    void resetRepositories();
    void loadLookupModels();
    void syncContentStatusModels();
    void refreshInbox();
    void refreshAllContent();
    void refreshBoard();
    void refreshCalendar();
    void refreshSources();
    void refreshDerivatives();
    void refreshSeries();
    void refreshDashboard();
    void refreshClipboardHasText();
    void setStatusMessage(const QString &message);
    bool createIdeaFromTextInternal(const QString &text, QString *errorMessage = nullptr);
    [[nodiscard]] QVariantList mediaVariantList(const std::vector<Domain::MediaItem> &items) const;
    [[nodiscard]] std::vector<Domain::MediaItem> prepareMediaItems(const QVariantList &items,
                                                                   const QString &mediaDataDir,
                                                                   bool fetchUrlTitles,
                                                                   QString *errorMessage) const;
    [[nodiscard]] QString fetchUrlTitle(const QString &url) const;
    [[nodiscard]] QString resolveMediaName(const QVariantMap &item,
                                           const QString &sourceType,
                                           bool fetchUrlTitles) const;
    [[nodiscard]] QString copyMediaFile(const QString &sourcePath,
                                        const QString &mediaDataDir,
                                        QString *errorMessage) const;

    Data::Database database_;
    std::unique_ptr<Data::LookupsRepository> lookupsRepository_;
    std::unique_ptr<Data::MediaRepository> mediaRepository_;
    std::unique_ptr<Data::PublicationRepository> publicationRepository_;
    std::unique_ptr<Data::SeriesRepository> seriesRepository_;
    std::unique_ptr<Data::ContentRepository> contentRepository_;
    std::unique_ptr<Data::DashboardRepository> dashboardRepository_;

    Models::ContentListModel inboxModel_;
    Models::ContentStatusListModel contentStatusModel_;
    Models::CalendarEntryModel calendarModel_;
    Models::ContentListModel allContentModel_;
    Models::ContentListModel sourceModel_;
    Models::ContentListModel derivativeModel_;
    Models::SeriesListModel seriesModel_;
    Models::LookupListModel pillarModel_;
    Models::LookupListModel kindModel_;
    Models::LookupListModel channelModel_;
    Models::DashboardMetricModel dashboardByPillarModel_;
    Models::DashboardMetricModel dashboardBySeriesModel_;
    Models::DashboardMetricModel dashboardByStatusModel_;
    Models::DashboardMetricModel dashboardUpcomingModel_;
    Models::DashboardMetricModel dashboardPublishedContentModel_;
    Models::DashboardMetricModel dashboardPublishedPublicationsModel_;
    Models::DashboardMetricModel dashboardZeroPublishedPillarsModel_;

    bool boardShowArchived_ = false;
    bool allContentShowArchived_ = false;
    bool dashboardIncludeArchived_ = false;
    bool calendarIncludeArchived_ = false;
    bool calendarIncludePublished_ = false;
    bool clipboardHasText_ = false;
    QString currentSourceId_;
    QString searchQuery_;
    QString statusMessage_;
    int descriptionPreviewWordCap_ = 20;
    Data::ContentRepository::SortMode allContentSortMode_ = Data::ContentRepository::SortMode::DueDateAlphabetical;
    std::map<QString, std::unique_ptr<Models::ContentListModel>> boardModels_;
};

} // namespace SmTool::App
