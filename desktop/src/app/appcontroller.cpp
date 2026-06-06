#include "app/appcontroller.h"

#include "app/loggingcontroller.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlError>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUuid>

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

bool isMarkdownFilePath(const QString &filePath)
{
    const auto suffix = QFileInfo{filePath}.suffix().toLower();
    return suffix == "md"_L1 || suffix == "markdown"_L1;
}

bool isRemoteUrl(const QString &value)
{
    const auto url = QUrl{value.trimmed()};
    return url.isValid() && (url.scheme() == "http"_L1 || url.scheme() == "https"_L1);
}

QString localFilePathFromUrl(const QString &urlText)
{
    const auto url = QUrl{urlText.trimmed()};
    return url.isLocalFile() ? url.toLocalFile() : QString{};
}

QString fileNameFromPath(const QString &path)
{
    return QFileInfo{path}.fileName().trimmed();
}

QString htmlTitle(const QString &html)
{
    static const QRegularExpression titlePattern(QStringLiteral(R"(<title[^>]*>(.*?)</title>)"),
                                                 QRegularExpression::CaseInsensitiveOption
                                                     | QRegularExpression::DotMatchesEverythingOption);
    auto title = titlePattern.match(html).captured(1);
    title.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return title.trimmed();
}

bool beginSavepoint(const QSqlDatabase &db, const QString &name, QString *errorMessage)
{
    QSqlQuery query{db};
    if (query.exec(QStringLiteral("SAVEPOINT %1").arg(name))) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

void rollbackSavepoint(const QSqlDatabase &db, const QString &name)
{
    QSqlQuery query{db};
    query.exec(QStringLiteral("ROLLBACK TO SAVEPOINT %1").arg(name));
    query.exec(QStringLiteral("RELEASE SAVEPOINT %1").arg(name));
}

bool releaseSavepoint(const QSqlDatabase &db, const QString &name, QString *errorMessage)
{
    QSqlQuery query{db};
    if (query.exec(QStringLiteral("RELEASE SAVEPOINT %1").arg(name))) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

QStringList splitIdeasFromMarkdown(const QString &text)
{
    const auto sections = text.split(QRegularExpression(QStringLiteral(R"((?:\r?\n)\s*---\s*(?:\r?\n))")));
    QStringList ideas;
    for (const auto &section : sections) {
        const auto trimmed = section.trimmed();
        if (!trimmed.isEmpty()) {
            ideas.append(trimmed);
        }
    }
    return ideas;
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
    descriptionPreviewWordCap_ = value;
    inboxModel_.setDescriptionPreviewWordCap(value);
    for (auto &[statusId, model] : boardModels_) {
        Q_UNUSED(statusId);
        model->setDescriptionPreviewWordCap(value);
    }
    allContentModel_.setDescriptionPreviewWordCap(value);
    sourceModel_.setDescriptionPreviewWordCap(value);
    derivativeModel_.setDescriptionPreviewWordCap(value);
}

Models::ContentListModel *AppController::inboxModel() { return &inboxModel_; }
Models::ContentStatusListModel *AppController::contentStatusModel() { return &contentStatusModel_; }
Models::CalendarEntryModel *AppController::calendarModel() { return &calendarModel_; }
Models::ContentListModel *AppController::allContentModel() { return &allContentModel_; }
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

bool AppController::allContentShowArchived() const { return allContentShowArchived_; }

void AppController::setAllContentShowArchived(bool enabled)
{
    if (allContentShowArchived_ == enabled) {
        return;
    }
    allContentShowArchived_ = enabled;
    refreshAllContent();
    emit allContentShowArchivedChanged();
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

QString AppController::searchQuery() const { return searchQuery_; }

void AppController::setSearchQuery(const QString &value)
{
    const auto normalized = value;
    if (searchQuery_ == normalized) {
        return;
    }
    searchQuery_ = normalized;
    refreshAll();
    emit searchQueryChanged();
}

QString AppController::statusMessage() const { return statusMessage_; }

int AppController::allContentSortMode() const
{
    return static_cast<int>(allContentSortMode_);
}

void AppController::setAllContentSortMode(int mode)
{
    const auto normalized = static_cast<Data::ContentRepository::SortMode>(mode);
    if (allContentSortMode_ == normalized) {
        return;
    }
    allContentSortMode_ = normalized;
    refreshAllContent();
}

void AppController::clearSearchQuery()
{
    setSearchQuery(QString{});
}

bool AppController::refreshAll()
{
    loadLookupModels();
    refreshInbox();
    refreshAllContent();
    refreshBoard();
    refreshCalendar();
    refreshSources();
    refreshDerivatives();
    refreshSeries();
    refreshDashboard();
    return true;
}

QObject *AppController::boardModelForStatus(const QString &statusId) const
{
    const auto it = boardModels_.find(statusId);
    return it != boardModels_.end() ? it->second.get() : nullptr;
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
        LOG_ERROR << "Clipboard import failed: clipboard is not available";
        return false;
    }

    const auto imported = createIdeaFromText(clipboard->text());
    if (imported) {
        LOG_INFO << "1 idea was imported from clipboard";
    } else {
        LOG_ERROR << "Clipboard import failed: " << statusMessage_.toStdString();
    }
    return imported;
}

bool AppController::createIdeaFromText(const QString &text)
{
    QString errorMessage;
    const auto created = createIdeaFromTextInternal(text, &errorMessage);
    setStatusMessage(created ? QStringLiteral("Inbox item created.") : errorMessage);
    return created;
}

bool AppController::importIdeasFromUserSelectedFile()
{
    const auto selectedPath = QFileDialog::getOpenFileName(nullptr,
                                                           QStringLiteral("Import Ideas"),
                                                           QString{},
                                                           QStringLiteral("Text and Markdown files (*.txt *.md *.markdown);;All files (*)"));
    if (selectedPath.isEmpty()) {
        return false;
    }

    return importIdeasFromFile(selectedPath);
}

bool AppController::importIdeasFromFile(const QString &filePath)
{
    const auto trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        setStatusMessage(QStringLiteral("No import file selected."));
        LOG_ERROR << "File import failed: no file path was selected";
        return false;
    }

    QFile file{trimmedPath};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatusMessage(QStringLiteral("Could not open import file."));
        LOG_ERROR << "File import failed for '" << trimmedPath.toStdString()
                  << "': " << file.errorString().toStdString();
        return false;
    }

    const auto content = QString::fromUtf8(file.readAll());
    const auto ideas = isMarkdownFilePath(trimmedPath) ? splitIdeasFromMarkdown(content)
                                                       : QStringList{content.trimmed()};

    int importedCount = 0;
    for (const auto &idea : ideas) {
        QString errorMessage;
        if (!createIdeaFromTextInternal(idea, &errorMessage)) {
            setStatusMessage(errorMessage);
            LOG_ERROR << "File import failed for '" << trimmedPath.toStdString()
                      << "': " << errorMessage.toStdString();
            return false;
        }
        ++importedCount;
    }

    if (importedCount == 0) {
        setStatusMessage(QStringLiteral("Import file did not contain any ideas."));
        LOG_ERROR << "File import failed for '" << trimmedPath.toStdString()
                  << "': import file did not contain any ideas";
        return false;
    }

    setStatusMessage(importedCount == 1
                         ? QStringLiteral("Imported 1 idea from file.")
                         : QStringLiteral("Imported %1 ideas from file.").arg(importedCount));
    LOG_INFO << importedCount << (importedCount == 1 ? " idea was imported from: " : " ideas were imported from: ")
             << trimmedPath.toStdString();
    return true;
}

QString AppController::chooseMediaFile() const
{
    return QFileDialog::getOpenFileName(nullptr,
                                        QStringLiteral("Select Media File"),
                                        QString{},
                                        QStringLiteral("All files (*)"));
}

QString AppController::localPathFromUrl(const QString &urlText) const
{
    return localFilePathFromUrl(urlText);
}

bool AppController::openMedia(const QVariantMap &mediaItem, const QString &mediaDataDir) const
{
    const auto sourceType = mediaItem.value(QStringLiteral("sourceType")).toString().trimmed();
    const auto location = mediaItem.value(QStringLiteral("location")).toString().trimmed();
    if (sourceType.isEmpty() || location.isEmpty()) {
        return false;
    }

    if (sourceType == "url"_L1) {
        return QDesktopServices::openUrl(QUrl{location});
    }

    const auto resolvedPath = sourceType == "managed_file"_L1
        ? QDir{mediaDataDir.trimmed()}.filePath(location)
        : location;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo{resolvedPath}.absoluteFilePath()));
}

QString AppController::copyMediaFileToDataDir(const QString &sourcePath, const QString &mediaDataDir)
{
    QString errorMessage;
    const auto copiedPath = copyMediaFile(sourcePath, mediaDataDir, &errorMessage);
    if (copiedPath.isEmpty()) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not copy media file.") : errorMessage);
        LOG_ERROR << "Copy media file failed: " << statusMessage_.toStdString();
        return {};
    }

    setStatusMessage(QStringLiteral("Media file copied."));
    LOG_INFO << "Media file copied from '" << sourcePath.toStdString()
             << "' to '" << copiedPath.toStdString() << "'";
    return copiedPath;
}

void AppController::logDebug(const QString &message) const
{
    LOG_DEBUG << message.toStdString();
}

bool AppController::createIdeaFromTextInternal(const QString &text, QString *errorMessage)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Text does not contain any content.");
        }
        return false;
    }

    const auto title = ideaTitleFromText(trimmed);
    if (title.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not derive an idea title.");
        }
        return false;
    }

    const auto pillars = lookupsRepository_->activeLookups(QStringLiteral("pillar"));
    if (pillars.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No active pillars are available.");
        }
        return false;
    }

    LOG_INFO << "Creating inbox idea from text with title '" << title.toStdString() << "'";
    return createInboxItem(title, trimmed, {}, pillars.front().id, {}, 0, {}, {});
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
        {QStringLiteral("tags"), item.tags},
        {QStringLiteral("kindId"), item.kindId},
        {QStringLiteral("pillarId"), item.pillarId},
        {QStringLiteral("priority"), item.priority},
        {QStringLiteral("scheduledAt"), item.scheduledAt.isValid() ? item.scheduledAt.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("suggestedChannelId"), item.suggestedChannelId},
        {QStringLiteral("status"), item.status},
        {QStringLiteral("media"), mediaVariantList(mediaRepository_->listForContent(contentId))},
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
        {QStringLiteral("media"), mediaVariantList(mediaRepository_->listForPublication(publicationId))},
    };
}

bool AppController::updateContent(const QString &contentId,
                                  const QString &title,
                                  const QString &description,
                                  const QString &tags,
                                  const QString &kindId,
                                  const QString &pillarId,
                                  int priority,
                                  const QString &scheduledAt,
                                  const QString &suggestedChannelId,
                                  const QString &status,
                                  const QVariantList &mediaItems,
                                  const QString &mediaDataDir,
                                  bool fetchUrlTitles)
{
    LOG_DEBUG << "updateContent requested for contentId='" << contentId.toStdString()
              << "' title='" << title.toStdString()
              << "' status='" << status.toStdString()
              << "' mediaItems=" << mediaItems.size();
    if (contentId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Content id is required."));
        LOG_ERROR << "updateContent failed: content id is required";
        return false;
    }
    if (title.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Title is required."));
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "': title is required";
        return false;
    }

    const auto existing = contentRepository_->getById(contentId);
    if (existing.id.isEmpty()) {
        setStatusMessage(QStringLiteral("Content not found."));
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "': content not found";
        return false;
    }
    const auto scheduled = parseOptionalDateTime(scheduledAt);
    if (!scheduledAt.trimmed().isEmpty() && !scheduled.isValid()) {
        setStatusMessage(QStringLiteral("Invalid scheduled date."));
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "': invalid scheduled date '" << scheduledAt.toStdString() << "'";
        return false;
    }

    Domain::ContentItem updated = existing;
    updated.title = title.trimmed();
    updated.description = description.trimmed();
    updated.tags = tags;
    updated.kindId = kindId.trimmed().isEmpty() ? existing.kindId : kindId;
    updated.pillarId = pillarId;
    updated.priority = priority;
    updated.scheduledAt = scheduled;
    updated.suggestedChannelId = suggestedChannelId;
    updated.status = status;

    QString errorMessage;
    const auto preparedMedia = prepareMediaItems(mediaItems, mediaDataDir, fetchUrlTitles, &errorMessage);
    if (!errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "' while preparing media: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "updateContent prepared " << preparedMedia.size()
              << " media items for contentId='" << contentId.toStdString() << "'";

    auto db = database_.connection();
    if (!beginSavepoint(db, QStringLiteral("app_content_update"), &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not start content save.") : errorMessage);
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "' starting savepoint: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "updateContent savepoint started for contentId='" << contentId.toStdString() << "'";
    if (!contentRepository_->update(updated, &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_content_update"));
        setStatusMessage(errorMessage);
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "' in contentRepository_->update: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "updateContent repository update succeeded for contentId='" << contentId.toStdString() << "'";
    if (!mediaRepository_->replaceForContent(contentId, preparedMedia, &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_content_update"));
        setStatusMessage(errorMessage);
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "' in mediaRepository_->replaceForContent: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "updateContent media replace succeeded for contentId='" << contentId.toStdString() << "'";
    if (!releaseSavepoint(db, QStringLiteral("app_content_update"), &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_content_update"));
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not save content media.") : errorMessage);
        LOG_ERROR << "updateContent failed for contentId='" << contentId.toStdString()
                  << "' releasing savepoint: " << errorMessage.toStdString();
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Content updated."));
    LOG_DEBUG << "updateContent completed for contentId='" << contentId.toStdString() << "'";
    return true;
}

bool AppController::savePublication(const QString &contentId,
                                    const QString &publicationId,
                                    const QString &channelId,
                                    const QString &status,
                                    const QString &scheduledAt,
                                    const QString &publishedAt,
                                    const QString &url,
                                    const QVariantList &mediaItems,
                                    const QString &mediaDataDir,
                                    bool fetchUrlTitles)
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
    const auto preparedMedia = prepareMediaItems(mediaItems, mediaDataDir, fetchUrlTitles, &errorMessage);
    if (!errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
        return false;
    }

    auto db = database_.connection();
    if (!beginSavepoint(db, QStringLiteral("app_publication_save"), &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not start publication save.") : errorMessage);
        return false;
    }

    const auto resolvedPublicationId = publication.id.isEmpty()
        ? publicationRepository_->create(publication, &errorMessage)
        : (publicationRepository_->update(publication, &errorMessage) ? publication.id : QString{});
    if (resolvedPublicationId.isEmpty()) {
        rollbackSavepoint(db, QStringLiteral("app_publication_save"));
        setStatusMessage(errorMessage);
        return false;
    }
    if (!mediaRepository_->replaceForPublication(resolvedPublicationId, preparedMedia, &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_publication_save"));
        setStatusMessage(errorMessage);
        return false;
    }
    if (!releaseSavepoint(db, QStringLiteral("app_publication_save"), &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_publication_save"));
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not save publication media.") : errorMessage);
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
                                    const QString &tags,
                                    const QString &pillarId,
                                    const QString &seriesId,
                                    int priority,
                                    const QString &scheduledAt,
                                    const QString &suggestedChannelId,
                                    const QVariantList &mediaItems,
                                    const QString &mediaDataDir,
                                    bool fetchUrlTitles)
{
    LOG_DEBUG << "createInboxItem requested title='" << title.toStdString()
              << "' mediaItems=" << mediaItems.size();
    if (title.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Title is required."));
        LOG_ERROR << "createInboxItem failed: title is required";
        return false;
    }
    const auto scheduled = parseOptionalDateTime(scheduledAt);
    if (!scheduledAt.trimmed().isEmpty() && !scheduled.isValid()) {
        setStatusMessage(QStringLiteral("Invalid scheduled date."));
        LOG_ERROR << "createInboxItem failed: invalid scheduled date '" << scheduledAt.toStdString() << "'";
        return false;
    }

    QString errorMessage;
    const auto ideaKindId = lookupsRepository_->lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    if (ideaKindId.isEmpty()) {
        setStatusMessage(QStringLiteral("Missing seeded content kind 'idea'."));
        LOG_ERROR << "createInboxItem failed: missing seeded content kind 'idea'";
        return false;
    }

    Domain::ContentItem content{
        .title = title.trimmed(),
        .description = description.trimmed(),
        .tags = tags,
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

    const auto preparedMedia = prepareMediaItems(mediaItems, mediaDataDir, fetchUrlTitles, &errorMessage);
    if (!errorMessage.isEmpty()) {
        setStatusMessage(errorMessage);
        LOG_ERROR << "createInboxItem failed while preparing media: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "createInboxItem prepared " << preparedMedia.size() << " media items";

    auto db = database_.connection();
    if (!beginSavepoint(db, QStringLiteral("app_content_create"), &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not start content save.") : errorMessage);
        LOG_ERROR << "createInboxItem failed starting savepoint: " << errorMessage.toStdString();
        return false;
    }

    const auto contentId = contentRepository_->create(content, &errorMessage);
    if (contentId.isEmpty()) {
        rollbackSavepoint(db, QStringLiteral("app_content_create"));
        setStatusMessage(errorMessage);
        LOG_ERROR << "createInboxItem failed in contentRepository_->create: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "createInboxItem repository create succeeded with contentId='" << contentId.toStdString() << "'";
    if (!mediaRepository_->replaceForContent(contentId, preparedMedia, &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_content_create"));
        setStatusMessage(errorMessage);
        LOG_ERROR << "createInboxItem failed in mediaRepository_->replaceForContent: " << errorMessage.toStdString();
        return false;
    }
    LOG_DEBUG << "createInboxItem media replace succeeded for contentId='" << contentId.toStdString() << "'";
    if (!releaseSavepoint(db, QStringLiteral("app_content_create"), &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_content_create"));
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not save content media.") : errorMessage);
        LOG_ERROR << "createInboxItem failed releasing savepoint: " << errorMessage.toStdString();
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Inbox item created."));
    LOG_DEBUG << "createInboxItem completed with contentId='" << contentId.toStdString() << "'";
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

QVariantList AppController::mediaVariantList(const std::vector<Domain::MediaItem> &items) const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(items.size()));
    for (const auto &item : items) {
        result.push_back(QVariantMap{
            {QStringLiteral("id"), item.id},
            {QStringLiteral("contentId"), item.contentId},
            {QStringLiteral("publicationId"), item.publicationId},
            {QStringLiteral("name"), item.name},
            {QStringLiteral("sourceType"), item.sourceType},
            {QStringLiteral("location"), item.location},
        });
    }
    return result;
}

std::vector<Domain::MediaItem> AppController::prepareMediaItems(const QVariantList &items,
                                                                const QString &mediaDataDir,
                                                                bool fetchUrlTitles,
                                                                QString *errorMessage) const
{
    LOG_DEBUG << "prepareMediaItems called with items=" << items.size()
              << " mediaDataDir='" << mediaDataDir.toStdString()
              << "' fetchUrlTitles=" << (fetchUrlTitles ? "true" : "false");
    std::vector<Domain::MediaItem> prepared;
    prepared.reserve(static_cast<std::size_t>(items.size()));

    for (const auto &value : items) {
        const auto item = value.toMap();
        auto location = item.value(QStringLiteral("location")).toString().trimmed();
        if (location.isEmpty()) {
            LOG_DEBUG << "prepareMediaItems skipping empty location item";
            continue;
        }

        auto sourceType = item.value(QStringLiteral("sourceType")).toString().trimmed();
        const auto copyFile = item.value(QStringLiteral("copyFile")).toBool();
        if (isRemoteUrl(location)) {
            sourceType = QStringLiteral("url");
        } else if (copyFile) {
            sourceType = QStringLiteral("managed_file");
        } else if (sourceType.isEmpty()) {
            sourceType = QFileInfo{location}.isAbsolute() ? QStringLiteral("file") : QStringLiteral("managed_file");
        }

        if (sourceType == "managed_file"_L1 && QFileInfo{location}.isAbsolute()) {
            location = copyMediaFile(location, mediaDataDir, errorMessage);
            if (location.isEmpty()) {
                LOG_ERROR << "prepareMediaItems failed copying managed file from '"
                          << item.value(QStringLiteral("location")).toString().toStdString()
                          << "': " << (errorMessage != nullptr ? errorMessage->toStdString() : std::string{});
                return {};
            }
        } else if (sourceType != "url"_L1) {
            location = QDir::cleanPath(location);
        }

        const auto name = resolveMediaName(item, sourceType, fetchUrlTitles);
        LOG_DEBUG << "prepareMediaItems item resolved name='" << name.toStdString()
                  << "' sourceType='" << sourceType.toStdString()
                  << "' location='" << location.toStdString() << "'";
        prepared.push_back({
            .id = item.value(QStringLiteral("id")).toString().trimmed(),
            .name = name,
            .sourceType = sourceType,
            .location = location,
        });
    }

    LOG_DEBUG << "prepareMediaItems completed with prepared items=" << prepared.size();
    return prepared;
}

QString AppController::fetchUrlTitle(const QString &url) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{url}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = manager.get(request);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    const auto timedOut = timeout.isActive() == false && reply->isFinished() == false;
    if (timedOut) {
        reply->abort();
    }

    const auto title = (!timedOut && reply->error() == QNetworkReply::NoError)
        ? htmlTitle(QString::fromUtf8(reply->readAll()))
        : QString{};
    reply->deleteLater();
    return title;
}

QString AppController::resolveMediaName(const QVariantMap &item,
                                        const QString &sourceType,
                                        bool fetchUrlTitles) const
{
    const auto explicitName = item.value(QStringLiteral("name")).toString().trimmed();
    const auto hasReusableExplicitName = !explicitName.isEmpty()
        && !(sourceType == "url"_L1 && explicitName.compare(QStringLiteral("Unnamed"), Qt::CaseInsensitive) == 0);
    if (hasReusableExplicitName) {
        return explicitName;
    }

    const auto location = item.value(QStringLiteral("location")).toString().trimmed();
    if (sourceType == "url"_L1) {
        if (fetchUrlTitles) {
            LOG_DEBUG << "resolveMediaName fetching title for url '" << location.toStdString() << "'";
            const auto title = fetchUrlTitle(location);
            if (!title.isEmpty()) {
                LOG_DEBUG << "resolveMediaName fetched title '" << title.toStdString()
                          << "' for url '" << location.toStdString() << "'";
                return title;
            }
            LOG_DEBUG << "resolveMediaName did not get a title for url '" << location.toStdString() << "'";
        }
        return QStringLiteral("Unnamed");
    }

    const auto fileName = fileNameFromPath(location);
    return fileName.isEmpty() ? QStringLiteral("Unnamed") : fileName;
}

QString AppController::copyMediaFile(const QString &sourcePath,
                                     const QString &mediaDataDir,
                                     QString *errorMessage) const
{
    const auto absoluteSourcePath = QFileInfo{sourcePath}.absoluteFilePath();
    const auto baseDir = mediaDataDir.trimmed();
    if (baseDir.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Media data directory is not configured.");
        }
        return {};
    }

    QDir dir{baseDir};
    if (!dir.mkpath(QStringLiteral("media"))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create media storage directory.");
        }
        return {};
    }

    const auto sourceInfo = QFileInfo{absoluteSourcePath};
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Media file does not exist.");
        }
        return {};
    }

    const auto relativeTargetPath = QDir{QStringLiteral("media")}.filePath(
        QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral("_") + sourceInfo.fileName());
    const auto absoluteTargetPath = dir.filePath(relativeTargetPath);
    if (!QFile::copy(absoluteSourcePath, absoluteTargetPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not copy media file.");
        }
        return {};
    }

    return relativeTargetPath;
}

void AppController::initializeRepositories()
{
    auto db = database_.connection();
    lookupsRepository_ = std::make_unique<Data::LookupsRepository>(db);
    mediaRepository_ = std::make_unique<Data::MediaRepository>(db);
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
    mediaRepository_.reset();
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
    syncContentStatusModels();
}

void AppController::syncContentStatusModels()
{
    if (!lookupsRepository_) {
        return;
    }

    auto statuses = lookupsRepository_->contentStatuses();
    contentStatusModel_.setItems(statuses);

    std::map<QString, std::unique_ptr<Models::ContentListModel>> nextModels;
    for (const auto &status : statuses) {
        if (auto existing = boardModels_.extract(status.id); !existing.empty()) {
            nextModels.emplace(status.id, std::move(existing.mapped()));
            continue;
        }

        auto model = std::make_unique<Models::ContentListModel>();
        model->setDescriptionPreviewWordCap(descriptionPreviewWordCap_);
        nextModels.emplace(status.id, std::move(model));
    }

    boardModels_ = std::move(nextModels);
}

void AppController::refreshInbox()
{
    inboxModel_.setItems(contentRepository_->inboxItems(searchQuery_));
}

void AppController::refreshAllContent()
{
    allContentModel_.setItems(contentRepository_->allItems(allContentShowArchived_, allContentSortMode_, searchQuery_));
}

void AppController::refreshBoard()
{
    for (const auto &status : contentStatusModel_.items()) {
        const auto it = boardModels_.find(status.id);
        if (it == boardModels_.end()) {
            continue;
        }
        it->second->setItems(contentRepository_->boardItems(status.id, boardShowArchived_, searchQuery_));
    }
}

void AppController::refreshCalendar()
{
    calendarModel_.setItems(dashboardRepository_->calendarEntries(calendarIncludeArchived_, calendarIncludePublished_, searchQuery_));
}

void AppController::refreshSources()
{
    sourceModel_.setItems(contentRepository_->rootItems(boardShowArchived_, searchQuery_));
    if (currentSourceId_.isEmpty() && sourceModel_.rowCount() > 0) {
        currentSourceId_ = sourceModel_.data(sourceModel_.index(0, 0), Models::ContentListModel::IdRole).toString();
        emit currentSourceIdChanged();
    }
}

void AppController::refreshDerivatives()
{
    derivativeModel_.setItems(currentSourceId_.isEmpty() ? std::vector<Domain::ContentSummary>{}
                                                         : contentRepository_->childItems(currentSourceId_, searchQuery_));
}

void AppController::refreshSeries()
{
    seriesModel_.setItems(seriesRepository_->list(boardShowArchived_, searchQuery_));
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
