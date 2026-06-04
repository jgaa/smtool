#include "app/appcontroller.h"

#include "app/loggingcontroller.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QMimeData>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::App {
namespace {

QString normalizedWhitespace(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

QString clipToWords(const QString &text, int maxWords)
{
    const auto words = normalizedWhitespace(text).split(u' ', Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return {};
    }

    QStringList clipped;
    clipped.reserve(std::min(static_cast<qsizetype>(maxWords), words.size()));
    for (int index = 0; index < words.size() && index < maxWords; ++index) {
        clipped.append(words.at(index));
    }
    return clipped.join(u' ');
}

QString firstSentence(const QString &text)
{
    const QRegularExpression sentencePattern(QStringLiteral(R"(^\s*(.+?[.!?])(?:\s|$))"),
                                             QRegularExpression::DotMatchesEverythingOption);
    const auto match = sentencePattern.match(text);
    if (match.hasMatch()) {
        return normalizedWhitespace(match.captured(1));
    }

    return normalizedWhitespace(text.section(u'\n', 0, 0));
}

bool looksLikeMarkdown(const QString &text)
{
    return text.contains(QRegularExpression(QStringLiteral(R"((^|\n)\s{0,3}(#|[-*+] |\d+\. |```|> ))")));
}

QString markdownTopic(const QString &text)
{
    const auto lines = text.split(u'\n');
    for (const auto &line : lines) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith(u'#')) {
            auto title = trimmed;
            title.remove(QRegularExpression(QStringLiteral(R"(^#+\s*)")));
            title.remove(QRegularExpression(QStringLiteral(R"(\s*#+$)")));
            return normalizedWhitespace(title);
        }
    }

    for (const auto &line : lines) {
        auto trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("```"))) {
            continue;
        }

        trimmed.remove(QRegularExpression(QStringLiteral(R"(^[-*+>]\s+)")));
        trimmed.remove(QRegularExpression(QStringLiteral(R"(^\d+\.\s+)")));
        trimmed.remove(QRegularExpression(QStringLiteral(R"(`+)")));
        trimmed.replace(QRegularExpression(QStringLiteral(R"(\[(.*?)\]\((.*?)\))")), QStringLiteral("\\1"));
        trimmed = normalizedWhitespace(trimmed);
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }

    return {};
}

QString ideaTitleFromText(const QString &text)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (looksLikeMarkdown(trimmed)) {
        const auto title = markdownTopic(trimmed);
        return title.isEmpty() ? clipToWords(firstSentence(trimmed), 5) : title;
    }

    return clipToWords(firstSentence(trimmed), 5);
}

QDateTime parseOptionalDateTime(const QString &value)
{
    const auto trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    for (const auto &format : {Qt::ISODate, Qt::ISODateWithMs}) {
        const auto parsed = QDateTime::fromString(trimmed, format);
        if (parsed.isValid()) {
            return parsed;
        }
    }

    for (const auto &format : {QStringLiteral("yyyy-MM-dd hh:mm"),
                               QStringLiteral("yyyy-MM-ddThh:mm"),
                               QStringLiteral("yyyy-MM-dd")}) {
        const auto parsed = QDateTime::fromString(trimmed, format);
        if (parsed.isValid()) {
            return parsed;
        }
    }

    return {};
}

} // namespace

AppController::AppController(Data::Database::Options databaseOptions, QObject *parent)
    : QObject(parent)
    , database_(std::move(databaseOptions))
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        connect(clipboard, &QClipboard::dataChanged, this, &AppController::refreshClipboardHasText);
        refreshClipboardHasText();
    }
}

bool AppController::initialize(QString *errorMessage)
{
    if (!database_.initialize(errorMessage)) {
        return false;
    }

    initializeRepositories();
    loadLookupModels();
    refreshAll();
    setStatusMessage(QStringLiteral("Database ready."));
    LOG_INFO << "SmTool initialized";
    return true;
}

void AppController::setDescriptionPreviewWordCap(int value)
{
    inboxModel_.setDescriptionPreviewWordCap(value);
    boardInboxModel_.setDescriptionPreviewWordCap(value);
    boardClarifyingModel_.setDescriptionPreviewWordCap(value);
    boardShapingModel_.setDescriptionPreviewWordCap(value);
    boardDraftingModel_.setDescriptionPreviewWordCap(value);
    boardReadyModel_.setDescriptionPreviewWordCap(value);
    boardScheduledModel_.setDescriptionPreviewWordCap(value);
    boardPublishedModel_.setDescriptionPreviewWordCap(value);
    boardReviewingModel_.setDescriptionPreviewWordCap(value);
    boardArchivedModel_.setDescriptionPreviewWordCap(value);
    sourceModel_.setDescriptionPreviewWordCap(value);
    derivativeModel_.setDescriptionPreviewWordCap(value);
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

bool AppController::calendarIncludeArchived() const { return calendarIncludeArchived_; }

void AppController::setCalendarIncludeArchived(bool enabled)
{
    if (calendarIncludeArchived_ == enabled) {
        return;
    }
    calendarIncludeArchived_ = enabled;
    refreshCalendar();
    emit calendarIncludeArchivedChanged();
}

bool AppController::calendarIncludePublished() const { return calendarIncludePublished_; }

void AppController::setCalendarIncludePublished(bool enabled)
{
    if (calendarIncludePublished_ == enabled) {
        return;
    }
    calendarIncludePublished_ = enabled;
    refreshCalendar();
    emit calendarIncludePublishedChanged();
}

bool AppController::clipboardHasText() const { return clipboardHasText_; }

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

bool AppController::applyDatabasePath(const QString &path)
{
    QString errorMessage;
    resetRepositories();
    if (!database_.reopenAtPath(path, &errorMessage)) {
        initializeRepositories();
        refreshAll();
        setStatusMessage(errorMessage);
        LOG_ERROR << "Failed to reopen database after path change: " << errorMessage.toStdString();
        return false;
    }

    initializeRepositories();
    refreshAll();
    setStatusMessage(QStringLiteral("Database path updated."));
    LOG_INFO << "Database path updated";
    return true;
}

void AppController::copyTextToClipboard(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

bool AppController::pasteClipboardToIdea()
{
    const auto *clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        setStatusMessage(QStringLiteral("Clipboard is not available."));
        return false;
    }

    return createIdeaFromText(clipboard->text());
}

bool AppController::createIdeaFromText(const QString &text)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        setStatusMessage(QStringLiteral("Clipboard does not contain text."));
        return false;
    }

    const auto title = ideaTitleFromText(trimmed);
    if (title.isEmpty()) {
        setStatusMessage(QStringLiteral("Could not derive an idea title."));
        return false;
    }

    const auto pillars = lookupsRepository_->activeLookups(QStringLiteral("pillar"));
    if (pillars.empty()) {
        setStatusMessage(QStringLiteral("No active pillars are available."));
        return false;
    }

    LOG_INFO << "Creating inbox idea from pasted text with title '" << title.toStdString() << "'";
    return createInboxItem(title, trimmed, pillars.front().id, {}, 0, {}, {});
}

QVariantMap AppController::contentDetails(const QString &contentId) const
{
    if (!contentRepository_) {
        return {};
    }

    const auto item = contentRepository_->getById(contentId);
    if (item.id.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("id"), item.id},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("description"), item.description},
        {QStringLiteral("pillarId"), item.pillarId},
        {QStringLiteral("priority"), item.priority},
        {QStringLiteral("scheduledAt"), item.scheduledAt.isValid() ? item.scheduledAt.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("suggestedChannelId"), item.suggestedChannelId},
        {QStringLiteral("status"), item.status},
        {QStringLiteral("publications"), [this, &contentId]() {
            QVariantList publications;
            for (const auto &publication : publicationRepository_->listForContent(contentId)) {
                publications.push_back(QVariantMap{
                    {QStringLiteral("id"), publication.id},
                    {QStringLiteral("contentId"), publication.contentId},
                    {QStringLiteral("channelId"), publication.channelId},
                    {QStringLiteral("channelName"), publication.channelName},
                    {QStringLiteral("status"), publication.status},
                    {QStringLiteral("scheduledAt"), publication.scheduledAt.isValid() ? publication.scheduledAt.toString(Qt::ISODate) : QString{}},
                    {QStringLiteral("publishedAt"), publication.publishedAt.isValid() ? publication.publishedAt.toString(Qt::ISODate) : QString{}},
                    {QStringLiteral("url"), publication.url},
                });
            }
            return publications;
        }()},
    };
}

QVariantMap AppController::publicationDetails(const QString &publicationId) const
{
    if (!publicationRepository_) {
        return {};
    }

    const auto publication = publicationRepository_->getById(publicationId);
    if (publication.id.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("id"), publication.id},
        {QStringLiteral("contentId"), publication.contentId},
        {QStringLiteral("channelId"), publication.channelId},
        {QStringLiteral("channelName"), publication.channelName},
        {QStringLiteral("status"), publication.status},
        {QStringLiteral("scheduledAt"), publication.scheduledAt.isValid() ? publication.scheduledAt.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("publishedAt"), publication.publishedAt.isValid() ? publication.publishedAt.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("url"), publication.url},
    };
}

bool AppController::updateContent(const QString &contentId,
                                  const QString &title,
                                  const QString &description,
                                  const QString &pillarId,
                                  int priority,
                                  const QString &scheduledAt,
                                  const QString &suggestedChannelId,
                                  const QString &status)
{
    if (contentId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Content id is required."));
        return false;
    }
    if (title.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Title is required."));
        return false;
    }

    const auto existing = contentRepository_->getById(contentId);
    if (existing.id.isEmpty()) {
        setStatusMessage(QStringLiteral("Content not found."));
        return false;
    }
    const auto scheduled = parseOptionalDateTime(scheduledAt);
    if (!scheduledAt.trimmed().isEmpty() && !scheduled.isValid()) {
        setStatusMessage(QStringLiteral("Invalid scheduled date."));
        return false;
    }

    Domain::ContentItem updated = existing;
    updated.title = title.trimmed();
    updated.description = description.trimmed();
    updated.pillarId = pillarId;
    updated.priority = priority;
    updated.scheduledAt = scheduled;
    updated.suggestedChannelId = suggestedChannelId;
    updated.status = status;

    QString errorMessage;
    if (!contentRepository_->update(updated, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Content updated."));
    return true;
}

bool AppController::savePublication(const QString &contentId,
                                    const QString &publicationId,
                                    const QString &channelId,
                                    const QString &status,
                                    const QString &scheduledAt,
                                    const QString &publishedAt,
                                    const QString &url)
{
    if (contentId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Content id is required."));
        return false;
    }
    if (channelId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Channel is required."));
        return false;
    }

    const auto scheduled = parseOptionalDateTime(scheduledAt);
    if (!scheduledAt.trimmed().isEmpty() && !scheduled.isValid()) {
        setStatusMessage(QStringLiteral("Invalid scheduled date/time."));
        return false;
    }

    const auto published = parseOptionalDateTime(publishedAt);
    if (!publishedAt.trimmed().isEmpty() && !published.isValid()) {
        setStatusMessage(QStringLiteral("Invalid published date/time."));
        return false;
    }

    Domain::Publication publication{
        .id = publicationId.trimmed(),
        .contentId = contentId.trimmed(),
        .channelId = channelId.trimmed(),
        .status = status.trimmed(),
        .scheduledAt = scheduled,
        .publishedAt = published,
        .url = url.trimmed(),
    };

    QString errorMessage;
    const bool ok = publication.id.isEmpty()
        ? !publicationRepository_->create(publication, &errorMessage).isEmpty()
        : publicationRepository_->update(publication, &errorMessage);
    if (!ok) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Publication saved."));
    return true;
}

bool AppController::deletePublication(const QString &publicationId)
{
    if (publicationId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Publication id is required."));
        return false;
    }

    QString errorMessage;
    if (!publicationRepository_->remove(publicationId, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Publication removed."));
    return true;
}

bool AppController::deleteContent(const QString &contentId)
{
    if (contentId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Content id is required."));
        return false;
    }

    QString errorMessage;
    if (!contentRepository_->remove(contentId, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    if (currentSourceId_ == contentId) {
        currentSourceId_.clear();
        emit currentSourceIdChanged();
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Content removed."));
    return true;
}

bool AppController::createInboxItem(const QString &title,
                                    const QString &description,
                                    const QString &pillarId,
                                    const QString &seriesId,
                                    int priority,
                                    const QString &scheduledAt,
                                    const QString &suggestedChannelId)
{
    if (title.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Title is required."));
        return false;
    }
    const auto scheduled = parseOptionalDateTime(scheduledAt);
    if (!scheduledAt.trimmed().isEmpty() && !scheduled.isValid()) {
        setStatusMessage(QStringLiteral("Invalid scheduled date."));
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
        .scheduledAt = scheduled,
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

QVariantList AppController::burstTemplateOptions() const
{
    QVariantList options;
    if (!contentRepository_) {
        return options;
    }

    for (const auto &item : contentRepository_->activeBurstTemplates()) {
        QVariantMap option;
        option.insert(QStringLiteral("key"), item.key);
        option.insert(QStringLiteral("displayName"), item.displayName);
        options.append(option);
    }
    return options;
}

bool AppController::createBurstForCurrentSource(const QVariantList &templateKeys)
{
    if (currentSourceId_.isEmpty()) {
        setStatusMessage(QStringLiteral("Select a source item first."));
        return false;
    }

    QStringList selectedTemplateKeys;
    selectedTemplateKeys.reserve(templateKeys.size());
    for (const auto &value : templateKeys) {
        const auto key = value.toString().trimmed();
        if (!key.isEmpty()) {
            selectedTemplateKeys.append(key);
        }
    }
    selectedTemplateKeys.removeDuplicates();

    QString errorMessage;
    if (!contentRepository_->createBurst(currentSourceId_, selectedTemplateKeys, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Burst generated."));
    return true;
}

void AppController::initializeRepositories()
{
    auto db = database_.connection();
    lookupsRepository_ = std::make_unique<Data::LookupsRepository>(db);
    publicationRepository_ = std::make_unique<Data::PublicationRepository>(db);
    seriesRepository_ = std::make_unique<Data::SeriesRepository>(db);
    contentRepository_ = std::make_unique<Data::ContentRepository>(db);
    dashboardRepository_ = std::make_unique<Data::DashboardRepository>(db);
}

void AppController::resetRepositories()
{
    dashboardRepository_.reset();
    contentRepository_.reset();
    seriesRepository_.reset();
    publicationRepository_.reset();
    lookupsRepository_.reset();
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
    calendarModel_.setItems(dashboardRepository_->calendarEntries(calendarIncludeArchived_, calendarIncludePublished_));
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

void AppController::refreshClipboardHasText()
{
    bool hasText = false;
    if (const auto *clipboard = QGuiApplication::clipboard()) {
        if (const auto *mimeData = clipboard->mimeData()) {
            hasText = mimeData->hasText();
        }
    }

    if (clipboardHasText_ == hasText) {
        return;
    }

    clipboardHasText_ = hasText;
    emit clipboardHasTextChanged();
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
