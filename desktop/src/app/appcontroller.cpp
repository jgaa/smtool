#include "app/appcontroller.h"

#include "app/loggingcontroller.h"
#include "domain/constants.h"

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
#include <QSet>
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

QString ideaTitleFromText(const QString &text, int wordCap)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (looksLikeMarkdown(trimmed)) {
        const auto title = markdownTopic(trimmed);
        return title.isEmpty() ? clipToWords(firstSentence(trimmed), wordCap) : title;
    }

    return clipToWords(firstSentence(trimmed), wordCap);
}

bool isMarkdownFilePath(const QString &filePath)
{
    const auto suffix = QFileInfo{filePath}.suffix().toLower();
    return suffix == "md"_L1;
}

bool isPlainTextFilePath(const QString &filePath)
{
    return QFileInfo{filePath}.suffix().toLower() == "txt"_L1;
}

bool isSupportedIdeaImportFilePath(const QString &filePath)
{
    return isMarkdownFilePath(filePath) || isPlainTextFilePath(filePath);
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

QString metricTypeForScope(const QString &scopeType)
{
    return scopeType == "channel"_L1 ? QStringLiteral("publication_count")
                                     : QStringLiteral("content_count");
}

bool goalTypeNeedsTarget(const QString &goalType)
{
    return goalType == "count"_L1 || goalType == "cadence"_L1;
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
    seriesContentModel_.setDescriptionPreviewWordCap(value);
}

void AppController::setDefaultContentPriority(int value)
{
    defaultContentPriority_ = std::clamp(value, 0, 100);
}

void AppController::setBatchMarkdownImportsEnabled(bool enabled)
{
    batchMarkdownImportsEnabled_ = enabled;
}

void AppController::setImportedIdeaTitleWordCap(int value)
{
    importedIdeaTitleWordCap_ = std::clamp(value, 1, 30);
}

Models::ContentListModel *AppController::inboxModel() { return &inboxModel_; }
Models::ContentStatusListModel *AppController::contentStatusModel() { return &contentStatusModel_; }
Models::CalendarEntryModel *AppController::calendarModel() { return &calendarModel_; }
Models::ContentListModel *AppController::allContentModel() { return &allContentModel_; }
Models::ContentListModel *AppController::sourceModel() { return &sourceModel_; }
Models::ContentListModel *AppController::derivativeModel() { return &derivativeModel_; }
Models::SeriesListModel *AppController::seriesModel() { return &seriesModel_; }
Models::ContentListModel *AppController::seriesContentModel() { return &seriesContentModel_; }
Models::LookupListModel *AppController::pillarModel() { return &pillarModel_; }
Models::LookupListModel *AppController::tagModel() { return &tagModel_; }
Models::LookupListModel *AppController::kindModel() { return &kindModel_; }
Models::LookupListModel *AppController::channelModel() { return &channelModel_; }
Models::LookupListModel *AppController::contentSeriesModel() { return &contentSeriesModel_; }
Models::LookupListModel *AppController::goalSeriesModel() { return &goalSeriesModel_; }
Models::GoalsListModel *AppController::goalsModel() { return &goalsModel_; }
Models::DashboardRowModel *AppController::goalAchievementModel() { return &goalAchievementModel_; }
Models::DashboardRowModel *AppController::pipelineCoverageModel() { return &pipelineCoverageModel_; }
Models::DashboardRowModel *AppController::balanceDeviationModel() { return &balanceDeviationModel_; }
Models::DashboardRowModel *AppController::dashboardAlertsModel() { return &dashboardAlertsModel_; }
Models::DashboardRowModel *AppController::recommendedFocusModel() { return &recommendedFocusModel_; }
Models::DashboardRowModel *AppController::dashboardStatisticsModel() { return &dashboardStatisticsModel_; }

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

bool AppController::seriesShowArchived() const { return seriesShowArchived_; }

void AppController::setSeriesShowArchived(bool enabled)
{
    if (seriesShowArchived_ == enabled) {
        return;
    }
    seriesShowArchived_ = enabled;
    refreshSeries();
    emit seriesShowArchivedChanged();
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

QString AppController::currentSeriesId() const { return currentSeriesId_; }

void AppController::setCurrentSeriesId(const QString &id)
{
    if (currentSeriesId_ == id) {
        return;
    }
    currentSeriesId_ = id;
    refreshSeriesContent();
    emit currentSeriesChanged();
}

QVariantMap AppController::currentSeriesDetails() const
{
    if (!seriesRepository_ || currentSeriesId_.isEmpty()) {
        return {};
    }

    const auto series = seriesRepository_->getById(currentSeriesId_);
    if (series.id.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("id"), series.id},
        {QStringLiteral("name"), series.name},
        {QStringLiteral("description"), series.description},
        {QStringLiteral("pillarId"), series.pillarId},
        {QStringLiteral("pillarName"), series.pillarName},
        {QStringLiteral("status"), series.status},
        {QStringLiteral("contentCount"), series.contentCount},
        {QStringLiteral("scheduledCount"), series.scheduledCount},
        {QStringLiteral("createdAt"), series.createdAt.isValid() ? series.createdAt.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("updatedAt"), series.updatedAt.isValid() ? series.updatedAt.toString(Qt::ISODate) : QString{}},
    };
}

QString AppController::seriesSearchQuery() const { return seriesSearchQuery_; }

void AppController::setSeriesSearchQuery(const QString &value)
{
    if (seriesSearchQuery_ == value) {
        return;
    }
    seriesSearchQuery_ = value;
    refreshSeries();
    emit seriesSearchQueryChanged();
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

QString AppController::dashboardPerformancePeriodKey() const { return dashboardPerformanceSelection_.key; }
QString AppController::dashboardPerformanceStartDate() const { return dashboardPerformanceSelection_.startDate; }
QString AppController::dashboardPerformanceEndDate() const { return dashboardPerformanceSelection_.endDate; }

QString AppController::dashboardPerformancePeriodLabel() const
{
    if (!dashboardService_) {
        return QStringLiteral("Last 90 days");
    }
    return dashboardService_->resolvePerformancePeriod(dashboardPerformanceSelection_).label;
}

QString AppController::dashboardPipelinePeriodKey() const { return dashboardPipelineSelection_.key; }
QString AppController::dashboardPipelineStartDate() const { return dashboardPipelineSelection_.startDate; }
QString AppController::dashboardPipelineEndDate() const { return dashboardPipelineSelection_.endDate; }

QString AppController::dashboardPipelinePeriodLabel() const
{
    if (!dashboardService_) {
        return QStringLiteral("Next 30 days");
    }
    return dashboardService_->resolvePipelinePeriod(dashboardPipelineSelection_).label;
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
    refreshSeriesContent();
    refreshGoals();
    refreshDashboard();
    return true;
}

void AppController::configureDashboardPerformancePeriod(const QString &key,
                                                        const QString &startDate,
                                                        const QString &endDate)
{
    dashboardPerformanceSelection_.key = key.trimmed().isEmpty() ? QStringLiteral("last_90_days") : key.trimmed();
    dashboardPerformanceSelection_.startDate = startDate.trimmed();
    dashboardPerformanceSelection_.endDate = endDate.trimmed();
    refreshDashboard();
    emit dashboardPeriodsChanged();
}

void AppController::configureDashboardPipelinePeriod(const QString &key,
                                                     const QString &startDate,
                                                     const QString &endDate)
{
    dashboardPipelineSelection_.key = key.trimmed().isEmpty() ? QStringLiteral("next_30_days") : key.trimmed();
    dashboardPipelineSelection_.startDate = startDate.trimmed();
    dashboardPipelineSelection_.endDate = endDate.trimmed();
    refreshDashboard();
    emit dashboardPeriodsChanged();
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
                                                           QStringLiteral("Text and Markdown files (*.txt *.md);;All files (*)"));
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

    if (!isSupportedIdeaImportFilePath(trimmedPath)) {
        setStatusMessage(QStringLiteral("Only .md and .txt files can be imported."));
        LOG_ERROR << "File import failed for '" << trimmedPath.toStdString()
                  << "': unsupported file extension";
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
    const auto ideas = isMarkdownFilePath(trimmedPath) && batchMarkdownImportsEnabled_
        ? splitIdeasFromMarkdown(content)
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

bool AppController::supportsIdeaImportFile(const QString &filePath) const
{
    return isSupportedIdeaImportFilePath(filePath.trimmed());
}

QString AppController::acceptableIdeaImportPath(const QVariantList &urls, const QString &text) const
{
    LOG_DEBUG << "Evaluating idea import drop: urls=" << urls.size()
              << " textLength=" << text.size();

    for (const auto &value : urls) {
        const auto urlText = value.toString().trimmed();
        const auto localPath = localFilePathFromUrl(urlText);
        LOG_DEBUG << "Drop URL '" << urlText.toStdString()
                  << "' localPath='" << localPath.toStdString() << "'";
        if (localPath.isEmpty()) {
            continue;
        }
        if (isSupportedIdeaImportFilePath(localPath)) {
            LOG_INFO << "Accepting dropped idea import file '" << localPath.toStdString() << "'";
            return localPath;
        }
        LOG_DEBUG << "Rejected dropped local file due to unsupported extension: '"
                  << localPath.toStdString() << "'";
    }

    const auto trimmedText = text.trimmed();
    if (!trimmedText.isEmpty()) {
        const auto firstLine = trimmedText.section(QRegularExpression(QStringLiteral("\\r?\\n")), 0, 0).trimmed();
        const auto localPath = localFilePathFromUrl(firstLine);
        LOG_DEBUG << "Drop text firstLine='" << firstLine.toStdString()
                  << "' localPath='" << localPath.toStdString() << "'";
        if (!localPath.isEmpty() && isSupportedIdeaImportFilePath(localPath)) {
            LOG_INFO << "Accepting dropped idea import file from text payload '"
                     << localPath.toStdString() << "'";
            return localPath;
        }
    }

    LOG_DEBUG << "Drop rejected for idea import: no acceptable .md or .txt local file found";
    return {};
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

bool AppController::contentHasDerivedItems(const QString &contentId) const
{
    if (!contentRepository_ || contentId.trimmed().isEmpty()) {
        return false;
    }
    return !contentRepository_->childItems(contentId).empty();
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

    const auto title = ideaTitleFromText(trimmed, importedIdeaTitleWordCap_);
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
    return createInboxItem(title, trimmed, {}, pillars.front().id, {}, defaultContentPriority_, {}, {});
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
        {QStringLiteral("seriesId"), item.seriesId},
        {QStringLiteral("seriesPosition"), item.hasSeriesPosition ? QVariant{item.seriesPosition} : QVariant{}},
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

QVariantList AppController::contentSeriesOptions() const
{
    QVariantList options;
    options.push_back(QVariantMap{
        {QStringLiteral("lookupId"), QString{}},
        {QStringLiteral("displayName"), QStringLiteral("None")},
    });

    if (!lookupsRepository_) {
        return options;
    }

    for (const auto &item : lookupsRepository_->series(false)) {
        options.push_back(QVariantMap{
            {QStringLiteral("lookupId"), item.id},
            {QStringLiteral("displayName"), item.displayName},
        });
    }
    return options;
}

QVariantList AppController::publicationFanOutOptions(const QString &contentId) const
{
    if (!lookupsRepository_ || !publicationRepository_) {
        return {};
    }

    QSet<QString> existingChannelIds;
    for (const auto &publication : publicationRepository_->listForContent(contentId.trimmed())) {
        existingChannelIds.insert(publication.channelId);
    }

    QVariantList options;
    for (const auto &item : lookupsRepository_->activeLookups(QStringLiteral("channel"))) {
        options.push_back(QVariantMap{
            {QStringLiteral("channelId"), item.id},
            {QStringLiteral("displayName"), item.displayName},
            {QStringLiteral("alreadyExists"), existingChannelIds.contains(item.id)},
        });
    }
    return options;
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

QVariantMap AppController::goalDetails(const QString &goalId) const
{
    if (!goalsRepository_) {
        return {};
    }

    const auto goal = goalsRepository_->getGoal(goalId);
    if (goal.id.isEmpty()) {
        return {};
    }

    QVariantList balanceItems;
    for (const auto &item : goalsRepository_->listBalanceItems(goalId)) {
        balanceItems.push_back(QVariantMap{
            {QStringLiteral("id"), item.id},
            {QStringLiteral("goalId"), item.goalId},
            {QStringLiteral("scopeType"), item.scopeType},
            {QStringLiteral("scopeId"), item.scopeId},
            {QStringLiteral("scopeDisplayName"), item.scopeDisplayName},
            {QStringLiteral("weight"), item.weight},
            {QStringLiteral("sortOrder"), item.sortOrder},
        });
    }

    return {
        {QStringLiteral("id"), goal.id},
        {QStringLiteral("name"), goal.name},
        {QStringLiteral("goalType"), goal.goalType},
        {QStringLiteral("scopeType"), goal.scopeType},
        {QStringLiteral("scopeId"), goal.scopeId},
        {QStringLiteral("scopeDisplayName"), goal.scopeDisplayName},
        {QStringLiteral("metricType"), goal.metricType},
        {QStringLiteral("targetValue"), goal.targetValue},
        {QStringLiteral("periodType"), goal.periodType},
        {QStringLiteral("periodValue"), goal.periodValue},
        {QStringLiteral("enabled"), goal.enabled},
        {QStringLiteral("summaryText"), goal.summaryText},
        {QStringLiteral("balanceItems"), balanceItems},
    };
}

QVariantList AppController::goalScopeOptions(const QString &scopeType) const
{
    if (!lookupsRepository_) {
        return {};
    }

    std::vector<Domain::LookupValue> items;
    if (scopeType == "pillar"_L1) {
        items = lookupsRepository_->activeLookups(QStringLiteral("pillar"));
    } else if (scopeType == "tag"_L1) {
        items = lookupsRepository_->tags();
    } else if (scopeType == "channel"_L1) {
        items = lookupsRepository_->activeLookups(QStringLiteral("channel"));
    } else if (scopeType == "series"_L1) {
        items = lookupsRepository_->series();
    } else if (scopeType == "kind"_L1) {
        items = lookupsRepository_->activeLookups(QStringLiteral("content_kind"));
    }

    QVariantList result;
    result.reserve(static_cast<qsizetype>(items.size()));
    for (const auto &item : items) {
        result.push_back(QVariantMap{
            {QStringLiteral("lookupId"), item.id},
            {QStringLiteral("key"), item.key},
            {QStringLiteral("displayName"), item.displayName},
            {QStringLiteral("description"), item.description},
            {QStringLiteral("sortOrder"), item.sortOrder},
            {QStringLiteral("isActive"), item.isActive},
        });
    }
    return result;
}

bool AppController::saveGoal(const QVariantMap &goalData, const QVariantList &balanceItems)
{
    if (!goalsRepository_) {
        setStatusMessage(QStringLiteral("Goals repository is not available."));
        return false;
    }

    Domain::Goal goal{
        .id = goalData.value(QStringLiteral("id")).toString().trimmed(),
        .name = goalData.value(QStringLiteral("name")).toString().trimmed(),
        .goalType = goalData.value(QStringLiteral("goalType")).toString().trimmed(),
        .scopeType = goalData.value(QStringLiteral("scopeType")).toString().trimmed(),
        .scopeId = goalData.value(QStringLiteral("scopeId")).toString().trimmed(),
        .metricType = goalData.value(QStringLiteral("metricType")).toString().trimmed(),
        .targetValue = goalData.value(QStringLiteral("targetValue")).toInt(),
        .periodType = goalData.value(QStringLiteral("periodType")).toString().trimmed(),
        .periodValue = goalData.value(QStringLiteral("periodValue")).toInt(),
        .enabled = goalData.value(QStringLiteral("enabled"), true).toBool(),
    };

    if (goal.name.isEmpty()) {
        setStatusMessage(QStringLiteral("Goal name is required."));
        return false;
    }
    if (!Domain::isValidGoalType(goal.goalType)) {
        setStatusMessage(QStringLiteral("Invalid goal type."));
        return false;
    }
    if (!Domain::isValidGoalScopeType(goal.scopeType)) {
        setStatusMessage(QStringLiteral("Invalid scope type."));
        return false;
    }

    if (goal.goalType == "balance"_L1) {
        goal.metricType = QStringLiteral("balance_weight");
        goal.targetValue = 0;
        goal.periodType.clear();
        goal.periodValue = 0;
        goal.scopeId.clear();
    } else {
        goal.metricType = metricTypeForScope(goal.scopeType);
        if (goal.scopeId.isEmpty() || !goalsRepository_->scopeExists(goal.scopeType, goal.scopeId)) {
            setStatusMessage(QStringLiteral("Select a valid goal target."));
            return false;
        }
        if (!goalTypeNeedsTarget(goal.goalType) || goal.targetValue <= 0) {
            setStatusMessage(QStringLiteral("Target value must be positive."));
            return false;
        }
        if (!Domain::isValidGoalPeriodType(goal.periodType) || goal.periodValue <= 0) {
            setStatusMessage(QStringLiteral("Select a valid period."));
            return false;
        }
    }

    std::vector<Domain::GoalBalanceItem> parsedBalanceItems;
    if (goal.goalType == "balance"_L1) {
        parsedBalanceItems.reserve(static_cast<std::size_t>(balanceItems.size()));
        int positiveCount = 0;
        for (const auto &value : balanceItems) {
            const auto itemMap = value.toMap();
            Domain::GoalBalanceItem item{
                .id = itemMap.value(QStringLiteral("id")).toString().trimmed(),
                .scopeType = goal.scopeType,
                .scopeId = itemMap.value(QStringLiteral("scopeId")).toString().trimmed(),
                .scopeDisplayName = itemMap.value(QStringLiteral("scopeDisplayName")).toString().trimmed(),
                .weight = std::max(0, itemMap.value(QStringLiteral("weight")).toInt()),
                .sortOrder = itemMap.value(QStringLiteral("sortOrder")).toInt(),
            };
            if (item.scopeId.isEmpty() || !goalsRepository_->scopeExists(goal.scopeType, item.scopeId)) {
                setStatusMessage(QStringLiteral("Balance goal contains an invalid scope item."));
                return false;
            }
            if (item.weight > 0) {
                ++positiveCount;
            }
            parsedBalanceItems.push_back(std::move(item));
        }
        if (positiveCount == 0) {
            setStatusMessage(QStringLiteral("Balance goals need at least one item with weight above zero."));
            return false;
        }
    }

    QString errorMessage;
    auto db = database_.connection();
    if (!beginSavepoint(db, QStringLiteral("app_goal_save"), &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not start goal save.") : errorMessage);
        return false;
    }

    const auto goalId = goal.id.isEmpty()
        ? goalsRepository_->createGoal(goal, &errorMessage)
        : (goalsRepository_->updateGoal(goal, &errorMessage) ? goal.id : QString{});
    if (goalId.isEmpty()) {
        rollbackSavepoint(db, QStringLiteral("app_goal_save"));
        setStatusMessage(errorMessage);
        return false;
    }
    if (goal.goalType == "balance"_L1) {
        if (!goalsRepository_->updateBalanceItems(goalId, goal.scopeType, parsedBalanceItems, &errorMessage)) {
            rollbackSavepoint(db, QStringLiteral("app_goal_save"));
            setStatusMessage(errorMessage);
            return false;
        }
    } else if (!goalsRepository_->updateBalanceItems(goalId, goal.scopeType, {}, &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_goal_save"));
        setStatusMessage(errorMessage);
        return false;
    }
    if (!releaseSavepoint(db, QStringLiteral("app_goal_save"), &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_goal_save"));
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not save goal.") : errorMessage);
        return false;
    }

    refreshGoals();
    refreshDashboard();
    setStatusMessage(goal.id.isEmpty() ? QStringLiteral("Goal created.") : QStringLiteral("Goal updated."));
    return true;
}

bool AppController::deleteGoal(const QString &goalId)
{
    if (goalId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Goal id is required."));
        return false;
    }

    QString errorMessage;
    if (!goalsRepository_->deleteGoal(goalId, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshGoals();
    refreshDashboard();
    setStatusMessage(QStringLiteral("Goal removed."));
    return true;
}

bool AppController::setGoalEnabled(const QString &goalId, bool enabled)
{
    if (goalId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Goal id is required."));
        return false;
    }

    QString errorMessage;
    if (!goalsRepository_->setGoalEnabled(goalId, enabled, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshGoals();
    refreshDashboard();
    setStatusMessage(enabled ? QStringLiteral("Goal enabled.") : QStringLiteral("Goal disabled."));
    return true;
}

bool AppController::updateContent(const QString &contentId,
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
    updated.seriesId = seriesId.trimmed();
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

bool AppController::createPublicationFanOut(const QString &contentId, const QVariantList &channelIds)
{
    if (!publicationRepository_) {
        setStatusMessage(QStringLiteral("Publication repository is not available."));
        return false;
    }

    QStringList selectedChannelIds;
    selectedChannelIds.reserve(channelIds.size());
    for (const auto &value : channelIds) {
        const auto channelId = value.toString().trimmed();
        if (!channelId.isEmpty()) {
            selectedChannelIds.append(channelId);
        }
    }
    selectedChannelIds.removeDuplicates();

    int createdCount = 0;
    QString errorMessage;
    auto db = database_.connection();
    if (!beginSavepoint(db, QStringLiteral("app_publication_fanout"), &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not start publication fan out.") : errorMessage);
        return false;
    }

    if (!publicationRepository_->createMissingForContent(contentId.trimmed(),
                                                         selectedChannelIds,
                                                         &createdCount,
                                                         &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_publication_fanout"));
        setStatusMessage(errorMessage);
        return false;
    }

    if (!releaseSavepoint(db, QStringLiteral("app_publication_fanout"), &errorMessage)) {
        rollbackSavepoint(db, QStringLiteral("app_publication_fanout"));
        setStatusMessage(errorMessage.isEmpty() ? QStringLiteral("Could not save publication fan out.") : errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(createdCount > 0
                         ? QStringLiteral("Created %1 publication alternative%2.")
                               .arg(createdCount)
                               .arg(createdCount == 1 ? QString{} : QStringLiteral("s"))
                         : QStringLiteral("No new publication alternatives were created."));
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

    const auto contentId = seriesId.trimmed().isEmpty()
        ? contentRepository_->create(content, &errorMessage)
        : contentRepository_->createInSeries(seriesId.trimmed(), content, &errorMessage);
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
    const auto seriesId = seriesRepository_->create(name.trimmed(), description.trimmed(), pillarId, QStringLiteral("active"), &errorMessage);
    if (seriesId.isEmpty()) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setCurrentSeriesId(seriesId);
    setStatusMessage(QStringLiteral("Series created."));
    return true;
}

bool AppController::saveSeries(const QString &seriesId,
                               const QString &name,
                               const QString &description,
                               const QString &pillarId,
                               const QString &status)
{
    if (name.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Series name is required."));
        return false;
    }

    QString errorMessage;
    if (seriesId.trimmed().isEmpty()) {
        const auto createdId = seriesRepository_->create(name.trimmed(), description.trimmed(), pillarId.trimmed(), status.trimmed(), &errorMessage);
        if (createdId.isEmpty()) {
            setStatusMessage(errorMessage);
            return false;
        }
        refreshAll();
        setCurrentSeriesId(createdId);
        setStatusMessage(QStringLiteral("Series created."));
        return true;
    }

    const auto existing = seriesRepository_->getById(seriesId.trimmed());
    if (existing.id.isEmpty()) {
        setStatusMessage(QStringLiteral("Series not found."));
        return false;
    }

    Domain::SeriesDetail updated = existing;
    updated.name = name.trimmed();
    updated.description = description.trimmed();
    updated.pillarId = pillarId.trimmed();
    updated.status = status.trimmed();
    if (!seriesRepository_->update(updated, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Series updated."));
    return true;
}

bool AppController::archiveCurrentSeries()
{
    if (currentSeriesId_.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Select a series first."));
        return false;
    }

    QString errorMessage;
    if (!seriesRepository_->archive(currentSeriesId_, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Series archived."));
    return true;
}

bool AppController::deleteCurrentSeries()
{
    if (currentSeriesId_.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Select a series first."));
        return false;
    }

    QString errorMessage;
    if (!seriesRepository_->remove(currentSeriesId_, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    const auto deletedId = currentSeriesId_;
    refreshAll();
    if (currentSeriesId_ == deletedId) {
        setCurrentSeriesId(QString{});
    }
    setStatusMessage(QStringLiteral("Series deleted."));
    return true;
}

bool AppController::moveSeriesContent(const QString &contentId, int direction)
{
    if (currentSeriesId_.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Select a series first."));
        return false;
    }

    QString errorMessage;
    if (!contentRepository_->moveSeriesItem(currentSeriesId_, contentId.trimmed(), direction, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshSeries();
    refreshSeriesContent();
    setStatusMessage(direction < 0 ? QStringLiteral("Series item moved up.") : QStringLiteral("Series item moved down."));
    return true;
}

bool AppController::removeContentFromCurrentSeries(const QString &contentId)
{
    if (contentId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Content id is required."));
        return false;
    }

    QString errorMessage;
    if (!contentRepository_->removeContentFromSeries(contentId.trimmed(), &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Content removed from series."));
    return true;
}

bool AppController::assignContentToCurrentSeries(const QString &contentId)
{
    if (currentSeriesId_.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Select a series first."));
        return false;
    }
    if (contentId.trimmed().isEmpty()) {
        setStatusMessage(QStringLiteral("Choose content to assign."));
        return false;
    }

    QString errorMessage;
    if (!contentRepository_->assignContentToSeries(contentId.trimmed(), currentSeriesId_, &errorMessage)) {
        setStatusMessage(errorMessage);
        return false;
    }

    refreshAll();
    setStatusMessage(QStringLiteral("Content assigned to series."));
    return true;
}

QVariantList AppController::assignableContentOptionsForCurrentSeries() const
{
    QVariantList options;
    if (!contentRepository_ || currentSeriesId_.isEmpty()) {
        return options;
    }

    for (const auto &item : contentRepository_->allItems(true, Data::ContentRepository::SortMode::Alphabetical)) {
        if (item.id.isEmpty()) {
            continue;
        }
        const auto content = contentRepository_->getById(item.id);
        if (content.seriesId == currentSeriesId_) {
            continue;
        }
        options.push_back(QVariantMap{
            {QStringLiteral("contentId"), item.id},
            {QStringLiteral("title"), item.title},
            {QStringLiteral("status"), item.status},
            {QStringLiteral("series"), item.seriesName},
        });
    }
    return options;
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
    calendarRepository_ = std::make_unique<Data::CalendarRepository>(db);
    mediaRepository_ = std::make_unique<Data::MediaRepository>(db);
    publicationRepository_ = std::make_unique<Data::PublicationRepository>(db);
    seriesRepository_ = std::make_unique<Data::SeriesRepository>(db);
    contentRepository_ = std::make_unique<Data::ContentRepository>(db);
    goalsRepository_ = std::make_unique<Data::GoalsRepository>(db);
    dashboardService_ = std::make_unique<DashboardService>(db);
}

void AppController::resetRepositories()
{
    dashboardService_.reset();
    contentRepository_.reset();
    seriesRepository_.reset();
    publicationRepository_.reset();
    mediaRepository_.reset();
    calendarRepository_.reset();
    lookupsRepository_.reset();
    goalsRepository_.reset();
}

void AppController::loadLookupModels()
{
    if (!lookupsRepository_) {
        return;
    }

    pillarModel_.setItems(lookupsRepository_->activeLookups(QStringLiteral("pillar")));
    tagModel_.setItems(lookupsRepository_->tags());
    kindModel_.setItems(lookupsRepository_->activeLookups(QStringLiteral("content_kind")));
    channelModel_.setItems(lookupsRepository_->activeLookups(QStringLiteral("channel")));
    contentSeriesModel_.setItems(lookupsRepository_->series(false));
    goalSeriesModel_.setItems(lookupsRepository_->series());
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
    calendarModel_.setItems(calendarRepository_ ? calendarRepository_->calendarEntries(calendarIncludeArchived_,
                                                                                       calendarIncludePublished_,
                                                                                       searchQuery_)
                                               : std::vector<Domain::CalendarEntry>{});
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
    seriesModel_.setItems(seriesRepository_->list(seriesShowArchived_, seriesSearchQuery_));
    if (currentSeriesId_.isEmpty() && seriesModel_.rowCount() > 0) {
        currentSeriesId_ = seriesModel_.data(seriesModel_.index(0, 0), Models::SeriesListModel::IdRole).toString();
        emit currentSeriesChanged();
    } else if (!currentSeriesId_.isEmpty()) {
        bool stillExists = false;
        for (int row = 0; row < seriesModel_.rowCount(); ++row) {
            if (seriesModel_.data(seriesModel_.index(row, 0), Models::SeriesListModel::IdRole).toString() == currentSeriesId_) {
                stillExists = true;
                break;
            }
        }
        if (!stillExists) {
            currentSeriesId_.clear();
            emit currentSeriesChanged();
        }
    }
}

void AppController::refreshSeriesContent()
{
    seriesContentModel_.setItems(currentSeriesId_.isEmpty() ? std::vector<Domain::ContentSummary>{}
                                                            : contentRepository_->listContentForSeries(currentSeriesId_));
}

void AppController::refreshGoals()
{
    goalsModel_.setItems(goalsRepository_ ? goalsRepository_->listGoals() : std::vector<Domain::Goal>{});
}

void AppController::refreshDashboard()
{
    if (!dashboardService_) {
        goalAchievementModel_.setItems({});
        pipelineCoverageModel_.setItems({});
        balanceDeviationModel_.setItems({});
        dashboardAlertsModel_.setItems({});
        recommendedFocusModel_.setItems({});
        dashboardStatisticsModel_.setItems({});
        return;
    }

    const auto evaluation = dashboardService_->evaluate(dashboardPerformanceSelection_, dashboardPipelineSelection_);
    goalAchievementModel_.setItems(evaluation.goalAchievement);
    pipelineCoverageModel_.setItems(evaluation.pipelineCoverage);
    balanceDeviationModel_.setItems(evaluation.balanceDeviation);
    dashboardAlertsModel_.setItems(evaluation.alerts);
    recommendedFocusModel_.setItems(evaluation.recommendedFocus);
    dashboardStatisticsModel_.setItems(evaluation.statistics);
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
