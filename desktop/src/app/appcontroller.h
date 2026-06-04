#pragma once

#include "data/contentrepository.h"
#include "data/dashboardrepository.h"
#include "data/database.h"
#include "data/lookupsrepository.h"
#include "data/publicationrepository.h"
#include "data/seriesrepository.h"
#include "models/calendarentrymodel.h"
#include "models/contentlistmodel.h"
#include "models/dashboardmetricmodel.h"
#include "models/lookuplistmodel.h"
#include "models/serieslistmodel.h"

#include <QObject>
#include <QVariantMap>

namespace SmTool::App {

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SmTool::Models::ContentListModel *inboxModel READ inboxModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardInboxModel READ boardInboxModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardClarifyingModel READ boardClarifyingModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardShapingModel READ boardShapingModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardDraftingModel READ boardDraftingModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardReadyModel READ boardReadyModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardScheduledModel READ boardScheduledModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardPublishedModel READ boardPublishedModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardReviewingModel READ boardReviewingModel CONSTANT)
    Q_PROPERTY(SmTool::Models::ContentListModel *boardArchivedModel READ boardArchivedModel CONSTANT)
    Q_PROPERTY(SmTool::Models::CalendarEntryModel *calendarModel READ calendarModel CONSTANT)
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
    Q_PROPERTY(bool dashboardIncludeArchived READ dashboardIncludeArchived WRITE setDashboardIncludeArchived NOTIFY dashboardIncludeArchivedChanged)
    Q_PROPERTY(bool calendarIncludeArchived READ calendarIncludeArchived WRITE setCalendarIncludeArchived NOTIFY calendarIncludeArchivedChanged)
    Q_PROPERTY(bool calendarIncludePublished READ calendarIncludePublished WRITE setCalendarIncludePublished NOTIFY calendarIncludePublishedChanged)
    Q_PROPERTY(bool clipboardHasText READ clipboardHasText NOTIFY clipboardHasTextChanged)
    Q_PROPERTY(QString currentSourceId READ currentSourceId WRITE setCurrentSourceId NOTIFY currentSourceIdChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit AppController(Data::Database::Options databaseOptions = {}, QObject *parent = nullptr);

    bool initialize(QString *errorMessage = nullptr);
    void setDescriptionPreviewWordCap(int value);

    [[nodiscard]] Models::ContentListModel *inboxModel();
    [[nodiscard]] Models::ContentListModel *boardInboxModel();
    [[nodiscard]] Models::ContentListModel *boardClarifyingModel();
    [[nodiscard]] Models::ContentListModel *boardShapingModel();
    [[nodiscard]] Models::ContentListModel *boardDraftingModel();
    [[nodiscard]] Models::ContentListModel *boardReadyModel();
    [[nodiscard]] Models::ContentListModel *boardScheduledModel();
    [[nodiscard]] Models::ContentListModel *boardPublishedModel();
    [[nodiscard]] Models::ContentListModel *boardReviewingModel();
    [[nodiscard]] Models::ContentListModel *boardArchivedModel();
    [[nodiscard]] Models::CalendarEntryModel *calendarModel();
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

    [[nodiscard]] bool dashboardIncludeArchived() const;
    void setDashboardIncludeArchived(bool enabled);

    [[nodiscard]] bool calendarIncludeArchived() const;
    void setCalendarIncludeArchived(bool enabled);

    [[nodiscard]] bool calendarIncludePublished() const;
    void setCalendarIncludePublished(bool enabled);

    [[nodiscard]] bool clipboardHasText() const;

    [[nodiscard]] QString currentSourceId() const;
    void setCurrentSourceId(const QString &id);

    [[nodiscard]] QString statusMessage() const;

    Q_INVOKABLE bool refreshAll();
    Q_INVOKABLE bool applyDatabasePath(const QString &path);
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE bool pasteClipboardToIdea();
    Q_INVOKABLE bool createIdeaFromText(const QString &text);
    Q_INVOKABLE QVariantMap contentDetails(const QString &contentId) const;
    Q_INVOKABLE QVariantMap publicationDetails(const QString &publicationId) const;
    Q_INVOKABLE bool updateContent(const QString &contentId,
                                   const QString &title,
                                   const QString &description,
                                   const QString &pillarId,
                                   int priority,
                                   const QString &scheduledAt,
                                   const QString &suggestedChannelId,
                                   const QString &status);
    Q_INVOKABLE bool savePublication(const QString &contentId,
                                     const QString &publicationId,
                                     const QString &channelId,
                                     const QString &status,
                                     const QString &scheduledAt,
                                     const QString &publishedAt,
                                     const QString &url);
    Q_INVOKABLE bool deletePublication(const QString &publicationId);
    Q_INVOKABLE bool deleteContent(const QString &contentId);
    Q_INVOKABLE bool createInboxItem(const QString &title,
                                     const QString &description,
                                     const QString &pillarId,
                                     const QString &seriesId,
                                     int priority,
                                     const QString &scheduledAt,
                                     const QString &suggestedChannelId);
    Q_INVOKABLE bool moveContentToStatus(const QString &contentId, const QString &targetStatus);
    Q_INVOKABLE bool createSeries(const QString &name, const QString &description, const QString &pillarId);
    Q_INVOKABLE QVariantList burstTemplateOptions() const;
    Q_INVOKABLE bool createBurstForCurrentSource(const QVariantList &templateKeys);

signals:
    void boardShowArchivedChanged();
    void dashboardIncludeArchivedChanged();
    void calendarIncludeArchivedChanged();
    void calendarIncludePublishedChanged();
    void clipboardHasTextChanged();
    void currentSourceIdChanged();
    void statusMessageChanged();

private:
    void initializeRepositories();
    void resetRepositories();
    void loadLookupModels();
    void refreshInbox();
    void refreshBoard();
    void refreshCalendar();
    void refreshSources();
    void refreshDerivatives();
    void refreshSeries();
    void refreshDashboard();
    void refreshClipboardHasText();
    void setStatusMessage(const QString &message);

    Data::Database database_;
    std::unique_ptr<Data::LookupsRepository> lookupsRepository_;
    std::unique_ptr<Data::PublicationRepository> publicationRepository_;
    std::unique_ptr<Data::SeriesRepository> seriesRepository_;
    std::unique_ptr<Data::ContentRepository> contentRepository_;
    std::unique_ptr<Data::DashboardRepository> dashboardRepository_;

    Models::ContentListModel inboxModel_;
    Models::ContentListModel boardInboxModel_;
    Models::ContentListModel boardClarifyingModel_;
    Models::ContentListModel boardShapingModel_;
    Models::ContentListModel boardDraftingModel_;
    Models::ContentListModel boardReadyModel_;
    Models::ContentListModel boardScheduledModel_;
    Models::ContentListModel boardPublishedModel_;
    Models::ContentListModel boardReviewingModel_;
    Models::ContentListModel boardArchivedModel_;
    Models::CalendarEntryModel calendarModel_;
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
    bool dashboardIncludeArchived_ = false;
    bool calendarIncludeArchived_ = false;
    bool calendarIncludePublished_ = false;
    bool clipboardHasText_ = false;
    QString currentSourceId_;
    QString statusMessage_;
};

} // namespace SmTool::App
