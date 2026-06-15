#pragma once

#include "app/dashboardservice.h"
#include "data/calendarrepository.h"
#include "data/contentrepository.h"
#include "data/database.h"
#include "data/goalsrepository.h"
#include "data/lookupsrepository.h"
#include "data/mediarepository.h"
#include "data/publicationrepository.h"
#include "data/seriesrepository.h"
#include "models/calendarentrymodel.h"
#include "models/contentlistmodel.h"
#include "models/contentstatuslistmodel.h"
#include "models/dashboardrowmodel.h"
#include "models/goalslistmodel.h"
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
    Q_PROPERTY(SmTool::Models::ContentListModel *seriesContentModel READ seriesContentModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *pillarModel READ pillarModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *tagModel READ tagModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *kindModel READ kindModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *channelModel READ channelModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *contentSeriesModel READ contentSeriesModel CONSTANT)
    Q_PROPERTY(SmTool::Models::LookupListModel *goalSeriesModel READ goalSeriesModel CONSTANT)
    Q_PROPERTY(SmTool::Models::GoalsListModel *goalsModel READ goalsModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardRowModel *goalAchievementModel READ goalAchievementModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardRowModel *pipelineCoverageModel READ pipelineCoverageModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardRowModel *balanceDeviationModel READ balanceDeviationModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardRowModel *dashboardAlertsModel READ dashboardAlertsModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardRowModel *recommendedFocusModel READ recommendedFocusModel CONSTANT)
    Q_PROPERTY(SmTool::Models::DashboardRowModel *dashboardStatisticsModel READ dashboardStatisticsModel CONSTANT)
    Q_PROPERTY(bool boardShowArchived READ boardShowArchived WRITE setBoardShowArchived NOTIFY boardShowArchivedChanged)
    Q_PROPERTY(bool allContentShowArchived READ allContentShowArchived WRITE setAllContentShowArchived NOTIFY allContentShowArchivedChanged)
    Q_PROPERTY(bool calendarIncludeArchived READ calendarIncludeArchived WRITE setCalendarIncludeArchived NOTIFY calendarIncludeArchivedChanged)
    Q_PROPERTY(bool calendarIncludePublished READ calendarIncludePublished WRITE setCalendarIncludePublished NOTIFY calendarIncludePublishedChanged)
    Q_PROPERTY(bool seriesShowArchived READ seriesShowArchived WRITE setSeriesShowArchived NOTIFY seriesShowArchivedChanged)
    Q_PROPERTY(bool clipboardHasText READ clipboardHasText NOTIFY clipboardHasTextChanged)
    Q_PROPERTY(QString currentSourceId READ currentSourceId WRITE setCurrentSourceId NOTIFY currentSourceIdChanged)
    Q_PROPERTY(QString currentSeriesId READ currentSeriesId WRITE setCurrentSeriesId NOTIFY currentSeriesChanged)
    Q_PROPERTY(QVariantMap currentSeriesDetails READ currentSeriesDetails NOTIFY currentSeriesChanged)
    Q_PROPERTY(QString seriesSearchQuery READ seriesSearchQuery WRITE setSeriesSearchQuery NOTIFY seriesSearchQueryChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString dashboardPerformancePeriodKey READ dashboardPerformancePeriodKey NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPerformanceStartDate READ dashboardPerformanceStartDate NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPerformanceEndDate READ dashboardPerformanceEndDate NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPerformancePeriodLabel READ dashboardPerformancePeriodLabel NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPipelinePeriodKey READ dashboardPipelinePeriodKey NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPipelineStartDate READ dashboardPipelineStartDate NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPipelineEndDate READ dashboardPipelineEndDate NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString dashboardPipelinePeriodLabel READ dashboardPipelinePeriodLabel NOTIFY dashboardPeriodsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit AppController(Data::Database::Options databaseOptions = {}, QObject *parent = nullptr);

    bool initialize(QString *errorMessage = nullptr);
    void setDefaultContentPriority(int value);
    void setBatchMarkdownImportsEnabled(bool enabled);
    void setImportedIdeaTitleWordCap(int value);
    void setDescriptionPreviewWordCap(int value);

    [[nodiscard]] Models::ContentListModel *inboxModel();
    [[nodiscard]] Models::ContentStatusListModel *contentStatusModel();
    [[nodiscard]] Models::CalendarEntryModel *calendarModel();
    [[nodiscard]] Models::ContentListModel *allContentModel();
    [[nodiscard]] Models::ContentListModel *sourceModel();
    [[nodiscard]] Models::ContentListModel *derivativeModel();
    [[nodiscard]] Models::SeriesListModel *seriesModel();
    [[nodiscard]] Models::ContentListModel *seriesContentModel();
    [[nodiscard]] Models::LookupListModel *pillarModel();
    [[nodiscard]] Models::LookupListModel *tagModel();
    [[nodiscard]] Models::LookupListModel *kindModel();
    [[nodiscard]] Models::LookupListModel *channelModel();
    [[nodiscard]] Models::LookupListModel *contentSeriesModel();
    [[nodiscard]] Models::LookupListModel *goalSeriesModel();
    [[nodiscard]] Models::GoalsListModel *goalsModel();
    [[nodiscard]] Models::DashboardRowModel *goalAchievementModel();
    [[nodiscard]] Models::DashboardRowModel *pipelineCoverageModel();
    [[nodiscard]] Models::DashboardRowModel *balanceDeviationModel();
    [[nodiscard]] Models::DashboardRowModel *dashboardAlertsModel();
    [[nodiscard]] Models::DashboardRowModel *recommendedFocusModel();
    [[nodiscard]] Models::DashboardRowModel *dashboardStatisticsModel();

    [[nodiscard]] bool boardShowArchived() const;
    void setBoardShowArchived(bool enabled);

    [[nodiscard]] bool allContentShowArchived() const;
    void setAllContentShowArchived(bool enabled);

    [[nodiscard]] bool calendarIncludeArchived() const;
    void setCalendarIncludeArchived(bool enabled);

    [[nodiscard]] bool calendarIncludePublished() const;
    void setCalendarIncludePublished(bool enabled);
    [[nodiscard]] bool seriesShowArchived() const;
    void setSeriesShowArchived(bool enabled);

    [[nodiscard]] bool clipboardHasText() const;

    [[nodiscard]] QString currentSourceId() const;
    void setCurrentSourceId(const QString &id);
    [[nodiscard]] QString currentSeriesId() const;
    void setCurrentSeriesId(const QString &id);
    [[nodiscard]] QVariantMap currentSeriesDetails() const;
    [[nodiscard]] QString seriesSearchQuery() const;
    void setSeriesSearchQuery(const QString &value);

    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString &value);
    [[nodiscard]] QString dashboardPerformancePeriodKey() const;
    [[nodiscard]] QString dashboardPerformanceStartDate() const;
    [[nodiscard]] QString dashboardPerformanceEndDate() const;
    [[nodiscard]] QString dashboardPerformancePeriodLabel() const;
    [[nodiscard]] QString dashboardPipelinePeriodKey() const;
    [[nodiscard]] QString dashboardPipelineStartDate() const;
    [[nodiscard]] QString dashboardPipelineEndDate() const;
    [[nodiscard]] QString dashboardPipelinePeriodLabel() const;

    [[nodiscard]] QString statusMessage() const;
    Q_INVOKABLE int allContentSortMode() const;
    Q_INVOKABLE void setAllContentSortMode(int mode);
    Q_INVOKABLE void clearSearchQuery();

    Q_INVOKABLE bool refreshAll();
    Q_INVOKABLE bool applyDatabasePath(const QString &path);
    Q_INVOKABLE void configureDashboardPerformancePeriod(const QString &key,
                                                         const QString &startDate = {},
                                                         const QString &endDate = {});
    Q_INVOKABLE void configureDashboardPipelinePeriod(const QString &key,
                                                      const QString &startDate = {},
                                                      const QString &endDate = {});
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE bool pasteClipboardToIdea();
    Q_INVOKABLE bool createIdeaFromText(const QString &text);
    Q_INVOKABLE bool importIdeasFromUserSelectedFile();
    Q_INVOKABLE bool importIdeasFromFile(const QString &filePath);
    Q_INVOKABLE bool supportsIdeaImportFile(const QString &filePath) const;
    Q_INVOKABLE QString acceptableIdeaImportPath(const QVariantList &urls, const QString &text) const;
    Q_INVOKABLE QString chooseMediaFile() const;
    Q_INVOKABLE QString localPathFromUrl(const QString &urlText) const;
    Q_INVOKABLE bool openMedia(const QVariantMap &mediaItem, const QString &mediaDataDir) const;
    Q_INVOKABLE QString copyMediaFileToDataDir(const QString &sourcePath, const QString &mediaDataDir);
    Q_INVOKABLE void logDebug(const QString &message) const;
    Q_INVOKABLE QObject *boardModelForStatus(const QString &statusId) const;
    Q_INVOKABLE QVariantMap contentDetails(const QString &contentId) const;
    Q_INVOKABLE QVariantList contentSeriesOptions() const;
    Q_INVOKABLE QVariantMap publicationDetails(const QString &publicationId) const;
    Q_INVOKABLE QVariantMap goalDetails(const QString &goalId) const;
    Q_INVOKABLE QVariantList goalScopeOptions(const QString &scopeType) const;
    Q_INVOKABLE bool saveGoal(const QVariantMap &goalData, const QVariantList &balanceItems);
    Q_INVOKABLE bool deleteGoal(const QString &goalId);
    Q_INVOKABLE bool setGoalEnabled(const QString &goalId, bool enabled);
    Q_INVOKABLE bool updateContent(const QString &contentId,
                                   const QString &title,
                                   const QString &description,
                                   const QString &tags,
                                   const QString &seriesId,
                                   const QString &kindId,
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
    Q_INVOKABLE bool saveSeries(const QString &seriesId,
                                const QString &name,
                                const QString &description,
                                const QString &pillarId,
                                const QString &status);
    Q_INVOKABLE bool archiveCurrentSeries();
    Q_INVOKABLE bool deleteCurrentSeries();
    Q_INVOKABLE bool moveSeriesContent(const QString &contentId, int direction);
    Q_INVOKABLE bool removeContentFromCurrentSeries(const QString &contentId);
    Q_INVOKABLE bool assignContentToCurrentSeries(const QString &contentId);
    Q_INVOKABLE QVariantList assignableContentOptionsForCurrentSeries() const;
    Q_INVOKABLE QVariantList burstTemplateOptions() const;
    Q_INVOKABLE bool createBurstForCurrentSource(const QVariantList &templateKeys);

signals:
    void boardShowArchivedChanged();
    void allContentShowArchivedChanged();
    void calendarIncludeArchivedChanged();
    void calendarIncludePublishedChanged();
    void seriesShowArchivedChanged();
    void clipboardHasTextChanged();
    void currentSourceIdChanged();
    void currentSeriesChanged();
    void seriesSearchQueryChanged();
    void searchQueryChanged();
    void dashboardPeriodsChanged();
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
    void refreshSeriesContent();
    void refreshGoals();
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
    DashboardService::PeriodSelection dashboardPerformanceSelection_{.key = QStringLiteral("last_90_days")};
    DashboardService::PeriodSelection dashboardPipelineSelection_{.key = QStringLiteral("next_30_days")};
    std::unique_ptr<Data::LookupsRepository> lookupsRepository_;
    std::unique_ptr<Data::CalendarRepository> calendarRepository_;
    std::unique_ptr<Data::MediaRepository> mediaRepository_;
    std::unique_ptr<Data::PublicationRepository> publicationRepository_;
    std::unique_ptr<Data::SeriesRepository> seriesRepository_;
    std::unique_ptr<Data::ContentRepository> contentRepository_;
    std::unique_ptr<Data::GoalsRepository> goalsRepository_;
    std::unique_ptr<DashboardService> dashboardService_;

    Models::ContentListModel inboxModel_;
    Models::ContentStatusListModel contentStatusModel_;
    Models::CalendarEntryModel calendarModel_;
    Models::ContentListModel allContentModel_;
    Models::ContentListModel sourceModel_;
    Models::ContentListModel derivativeModel_;
    Models::SeriesListModel seriesModel_;
    Models::ContentListModel seriesContentModel_;
    Models::LookupListModel pillarModel_;
    Models::LookupListModel tagModel_;
    Models::LookupListModel kindModel_;
    Models::LookupListModel channelModel_;
    Models::LookupListModel contentSeriesModel_;
    Models::LookupListModel goalSeriesModel_;
    Models::GoalsListModel goalsModel_;
    Models::DashboardRowModel goalAchievementModel_;
    Models::DashboardRowModel pipelineCoverageModel_;
    Models::DashboardRowModel balanceDeviationModel_;
    Models::DashboardRowModel dashboardAlertsModel_;
    Models::DashboardRowModel recommendedFocusModel_;
    Models::DashboardRowModel dashboardStatisticsModel_;

    bool boardShowArchived_ = false;
    bool allContentShowArchived_ = false;
    bool calendarIncludeArchived_ = false;
    bool calendarIncludePublished_ = false;
    bool seriesShowArchived_ = false;
    bool clipboardHasText_ = false;
    QString currentSourceId_;
    QString currentSeriesId_;
    QString seriesSearchQuery_;
    QString searchQuery_;
    QString statusMessage_;
    int defaultContentPriority_ = 5;
    bool batchMarkdownImportsEnabled_ = true;
    int importedIdeaTitleWordCap_ = 8;
    int descriptionPreviewWordCap_ = 20;
    Data::ContentRepository::SortMode allContentSortMode_ = Data::ContentRepository::SortMode::DueDateAlphabetical;
    std::map<QString, std::unique_ptr<Models::ContentListModel>> boardModels_;
};

} // namespace SmTool::App
