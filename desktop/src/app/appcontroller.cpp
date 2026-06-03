#include "app/appcontroller.h"

#include <QDateTime>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::App {

AppController::AppController(Data::Database::Options databaseOptions, QObject *parent)
    : QObject(parent)
    , database_(std::move(databaseOptions))
{
}

bool AppController::initialize(QString *errorMessage)
{
    if (!database_.initialize(errorMessage)) {
        return false;
    }

    auto db = database_.connection();
    lookupsRepository_ = std::make_unique<Data::LookupsRepository>(db);
    seriesRepository_ = std::make_unique<Data::SeriesRepository>(db);
    contentRepository_ = std::make_unique<Data::ContentRepository>(db);
    dashboardRepository_ = std::make_unique<Data::DashboardRepository>(db);

    loadLookupModels();
    refreshAll();
    setStatusMessage(QStringLiteral("Database ready: %1").arg(database_.databasePath()));
    return true;
}

Models::ContentListModel *AppController::inboxModel() { return &inboxModel_; }
Models::ContentListModel *AppController::boardInboxModel() { return &boardInboxModel_; }
Models::ContentListModel *AppController::boardClarifyingModel() { return &boardClarifyingModel_; }
Models::ContentListModel *AppController::boardShapingModel() { return &boardShapingModel_; }
Models::ContentListModel *AppController::boardDraftingModel() { return &boardDraftingModel_; }
Models::ContentListModel *AppController::boardReadyModel() { return &boardReadyModel_; }
Models::ContentListModel *AppController::boardScheduledModel() { return &boardScheduledModel_; }
Models::ContentListModel *AppController::boardPublishedModel() { return &boardPublishedModel_; }
Models::ContentListModel *AppController::boardReviewingModel() { return &boardReviewingModel_; }
Models::ContentListModel *AppController::boardArchivedModel() { return &boardArchivedModel_; }
Models::CalendarEntryModel *AppController::calendarModel() { return &calendarModel_; }
Models::ContentListModel *AppController::sourceModel() { return &sourceModel_; }
Models::ContentListModel *AppController::derivativeModel() { return &derivativeModel_; }
Models::SeriesListModel *AppController::seriesModel() { return &seriesModel_; }
Models::LookupListModel *AppController::pillarModel() { return &pillarModel_; }
Models::LookupListModel *AppController::kindModel() { return &kindModel_; }
Models::LookupListModel *AppController::channelModel() { return &channelModel_; }
Models::DashboardMetricModel *AppController::dashboardByPillarModel() { return &dashboardByPillarModel_; }
Models::DashboardMetricModel *AppController::dashboardBySeriesModel() { return &dashboardBySeriesModel_; }
Models::DashboardMetricModel *AppController::dashboardByStatusModel() { return &dashboardByStatusModel_; }
Models::DashboardMetricModel *AppController::dashboardUpcomingModel() { return &dashboardUpcomingModel_; }
Models::DashboardMetricModel *AppController::dashboardPublishedContentModel() { return &dashboardPublishedContentModel_; }
Models::DashboardMetricModel *AppController::dashboardPublishedPublicationsModel() { return &dashboardPublishedPublicationsModel_; }
Models::DashboardMetricModel *AppController::dashboardZeroPublishedPillarsModel() { return &dashboardZeroPublishedPillarsModel_; }

bool AppController::boardShowArchived() const { return boardShowArchived_; }

void AppController::setBoardShowArchived(bool enabled)
{
    if (boardShowArchived_ == enabled) {
        return;
    }
    boardShowArchived_ = enabled;
    refreshBoard();
    emit boardShowArchivedChanged();
}

bool AppController::dashboardIncludeArchived() const { return dashboardIncludeArchived_; }

void AppController::setDashboardIncludeArchived(bool enabled)
{
    if (dashboardIncludeArchived_ == enabled) {
        return;
    }
    dashboardIncludeArchived_ = enabled;
    refreshDashboard();
    emit dashboardIncludeArchivedChanged();
}

QString AppController::currentSourceId() const { return currentSourceId_; }

void AppController::setCurrentSourceId(const QString &id)
{
    if (currentSourceId_ == id) {
        return;
    }
    currentSourceId_ = id;
    refreshDerivatives();
    emit currentSourceIdChanged();
}

QString AppController::statusMessage() const { return statusMessage_; }

bool AppController::refreshAll()
{
    loadLookupModels();
    refreshInbox();
    refreshBoard();
    refreshCalendar();
    refreshSources();
    refreshDerivatives();
    refreshSeries();
    refreshDashboard();
    return true;
}

bool AppController::createInboxItem(const QString &title,
                                    const QString &description,
                                    const QString &pillarId,
                                    const QString &seriesId,
                                    int priority,
                                    const QString &suggestedChannelId)
{
    if (title.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Title is required."));
        return false;
    }

    QString errorMessage;
    const auto ideaKindId = lookupsRepository_->lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    if (ideaKindId.isEmpty()) {
        setStatusMessage(QStringLiteral("Missing seeded content kind 'idea'."));
        return false;
    }

    Domain::ContentItem content{
        .title = title.trimmed(),
        .description = description.trimmed(),
        .kindId = ideaKindId,
        .pillarId = pillarId,
        .suggestedChannelId = suggestedChannelId,
        .status = QStringLiteral("inbox"),
        .priority = priority,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    };
    content.seriesId = seriesId;

    if (contentRepository_->create(content, &errorMessage).isEmpty()) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Inbox item created."));
    return true;
}

bool AppController::moveContentToStatus(const QString &contentId, const QString &targetStatus)
{
    QString errorMessage;
    if (!contentRepository_->updateStatus(contentId, targetStatus, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Content moved to %1.").arg(targetStatus));
    return true;
}

bool AppController::createSeries(const QString &name, const QString &description, const QString &pillarId)
{
    if (name.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Series name is required."));
        return false;
    }

    QString errorMessage;
    if (seriesRepository_->create(name.trimmed(), description.trimmed(), pillarId, QStringLiteral("active"), &errorMessage).isEmpty()) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Series created."));
    return true;
}

bool AppController::createBurstForCurrentSource()
{
    if (currentSourceId_.isEmpty()) {
        setStatusMessage(QStringLiteral("Select a source item first."));
        return false;
    }

    QString errorMessage;
    if (!contentRepository_->createBurst(currentSourceId_, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Burst generated."));
    return true;
}

void AppController::loadLookupModels()
{
    if (!lookupsRepository_) {
        return;
    }

    pillarModel_.setItems(lookupsRepository_->activeLookups(QStringLiteral("pillar")));
    kindModel_.setItems(lookupsRepository_->activeLookups(QStringLiteral("content_kind")));
    channelModel_.setItems(lookupsRepository_->activeLookups(QStringLiteral("channel")));
}

void AppController::refreshInbox()
{
    inboxModel_.setItems(contentRepository_->inboxItems());
}

void AppController::refreshBoard()
{
    boardInboxModel_.setItems(contentRepository_->boardItems(QStringLiteral("inbox"), boardShowArchived_));
    boardClarifyingModel_.setItems(contentRepository_->boardItems(QStringLiteral("clarifying"), boardShowArchived_));
    boardShapingModel_.setItems(contentRepository_->boardItems(QStringLiteral("shaping"), boardShowArchived_));
    boardDraftingModel_.setItems(contentRepository_->boardItems(QStringLiteral("drafting"), boardShowArchived_));
    boardReadyModel_.setItems(contentRepository_->boardItems(QStringLiteral("ready"), boardShowArchived_));
    boardScheduledModel_.setItems(contentRepository_->boardItems(QStringLiteral("scheduled"), boardShowArchived_));
    boardPublishedModel_.setItems(contentRepository_->boardItems(QStringLiteral("published"), boardShowArchived_));
    boardReviewingModel_.setItems(contentRepository_->boardItems(QStringLiteral("reviewing"), boardShowArchived_));
    boardArchivedModel_.setItems(contentRepository_->boardItems(QStringLiteral("archived"), boardShowArchived_));
}

void AppController::refreshCalendar()
{
    calendarModel_.setItems(dashboardRepository_->calendarEntries());
}

void AppController::refreshSources()
{
    sourceModel_.setItems(contentRepository_->rootItems(boardShowArchived_));
    if (currentSourceId_.isEmpty() && sourceModel_.rowCount() > 0) {
        currentSourceId_ = sourceModel_.data(sourceModel_.index(0, 0), Models::ContentListModel::IdRole).toString();
        emit currentSourceIdChanged();
    }
}

void AppController::refreshDerivatives()
{
    derivativeModel_.setItems(currentSourceId_.isEmpty() ? std::vector<Domain::ContentSummary>{}
                                                         : contentRepository_->childItems(currentSourceId_));
}

void AppController::refreshSeries()
{
    seriesModel_.setItems(seriesRepository_->list(boardShowArchived_));
}

void AppController::refreshDashboard()
{
    const auto data = dashboardRepository_->dashboardData(dashboardIncludeArchived_);
    dashboardByPillarModel_.setItems(data.byPillar);
    dashboardBySeriesModel_.setItems(data.bySeries);
    dashboardByStatusModel_.setItems(data.byStatus);
    dashboardUpcomingModel_.setItems(data.upcoming);
    dashboardPublishedContentModel_.setItems(data.publishedContent);
    dashboardPublishedPublicationsModel_.setItems(data.publishedPublications);
    dashboardZeroPublishedPillarsModel_.setItems(data.zeroPublishedPillars);
}

void AppController::setStatusMessage(const QString &message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusMessageChanged();
}

} // namespace SmTool::App
