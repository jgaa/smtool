#include "data/contentrepository.h"

#include "domain/constants.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <ranges>
#include <set>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QVariant nullableString(const QString &value)
{
    return value.isEmpty() ? QVariant{} : QVariant{value};
}

QVariant nullableDateTime(const QDateTime &value)
{
    return value.isValid() ? QVariant{value.toString(Qt::ISODate)} : QVariant{};
}

QString normalizeTagToken(QString token)
{
    token = token.trimmed().toLower();
    while (token.startsWith(u'#')) {
        token.remove(0, 1);
    }

    static const QRegularExpression validPattern(QStringLiteral("^[a-z0-9_-]+$"));
    return validPattern.match(token).hasMatch() ? token : QString{};
}

QStringList normalizedTags(const QString &input)
{
    const auto rawTokens = input.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    std::set<QString> uniqueTags;
    for (const auto &rawToken : rawTokens) {
        const auto normalized = normalizeTagToken(rawToken);
        if (!normalized.isEmpty()) {
            uniqueTags.insert(normalized);
        }
    }

    QStringList tags;
    for (const auto &tag : uniqueTags) {
        tags.append(tag);
    }
    return tags;
}

QString joinedTags(const QStringList &tags)
{
    return tags.isEmpty() ? QStringLiteral("") : tags.join(u' ');
}

struct SearchTerms {
    enum class Scope {
        AllText,
        TitleOnly,
        DescriptionOnly,
    };

    Scope scope = Scope::AllText;
    QStringList textTerms;
    QStringList tagTerms;
};

SearchTerms parseSearchQuery(QString searchQuery)
{
    SearchTerms parsed;
    auto trimmed = searchQuery.trimmed();
    if (trimmed.startsWith(QStringLiteral("t:"), Qt::CaseInsensitive)) {
        parsed.scope = SearchTerms::Scope::TitleOnly;
        trimmed.remove(0, 2);
    } else if (trimmed.startsWith(QStringLiteral("title:"), Qt::CaseInsensitive)) {
        parsed.scope = SearchTerms::Scope::TitleOnly;
        trimmed.remove(0, 6);
    } else if (trimmed.startsWith(QStringLiteral("d:"), Qt::CaseInsensitive)) {
        parsed.scope = SearchTerms::Scope::DescriptionOnly;
        trimmed.remove(0, 2);
    } else if (trimmed.startsWith(QStringLiteral("description:"), Qt::CaseInsensitive)) {
        parsed.scope = SearchTerms::Scope::DescriptionOnly;
        trimmed.remove(0, 12);
    }

    for (const auto &token : trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        if (token.startsWith(u'#')) {
            const auto normalized = normalizeTagToken(token);
            if (!normalized.isEmpty()) {
                parsed.tagTerms.append(normalized);
            }
            continue;
        }

        parsed.textTerms.append(token.toLower());
    }
    return parsed;
}

bool containsAllTerms(const QString &haystack, const QStringList &terms)
{
    const auto lowered = haystack.toLower();
    return std::ranges::all_of(terms, [&](const auto &term) { return lowered.contains(term); });
}

bool matchesSearch(const Domain::ContentSummary &item, const SearchTerms &search)
{
    if (search.textTerms.isEmpty() && search.tagTerms.isEmpty()) {
        return true;
    }

    const auto tagSet = item.tags.split(u' ', Qt::SkipEmptyParts);
    if (!std::ranges::all_of(search.tagTerms, [&](const auto &tag) { return tagSet.contains(tag); })) {
        return false;
    }

    switch (search.scope) {
    case SearchTerms::Scope::TitleOnly:
        return containsAllTerms(item.title, search.textTerms);
    case SearchTerms::Scope::DescriptionOnly:
        return containsAllTerms(item.description, search.textTerms);
    case SearchTerms::Scope::AllText:
    default:
        return containsAllTerms(item.title + u' ' + item.description, search.textTerms);
    }
}

bool execWithError(QSqlQuery &query, QString *errorMessage)
{
    if (query.exec()) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool syncContentTags(QSqlDatabase db, const QString &contentId, const QString &tagsInput, QString *errorMessage)
{
    const auto tags = normalizedTags(tagsInput);

    QSqlQuery clearLinks{db};
    clearLinks.prepare(QStringLiteral("DELETE FROM content_tag WHERE content_id = :content_id"));
    clearLinks.bindValue(":content_id"_L1, contentId);
    if (!execWithError(clearLinks, errorMessage)) {
        return false;
    }

    for (const auto &tag : tags) {
        QSqlQuery insertTag{db};
        insertTag.prepare(QStringLiteral("INSERT OR IGNORE INTO tag (id) VALUES (:id)"));
        insertTag.bindValue(":id"_L1, tag);
        if (!execWithError(insertTag, errorMessage)) {
            return false;
        }

        QSqlQuery insertRef{db};
        insertRef.prepare(QStringLiteral(
            "INSERT INTO content_tag (content_id, tag_id) VALUES (:content_id, :tag_id)"));
        insertRef.bindValue(":content_id"_L1, contentId);
        insertRef.bindValue(":tag_id"_L1, tag);
        if (!execWithError(insertRef, errorMessage)) {
            return false;
        }
    }

    QSqlQuery updateCache{db};
    updateCache.prepare(QStringLiteral("UPDATE content SET tags_cache = :tags_cache WHERE id = :id"));
    updateCache.bindValue(":tags_cache"_L1, joinedTags(tags));
    updateCache.bindValue(":id"_L1, contentId);
    return execWithError(updateCache, errorMessage);
}

bool contentStatusExists(const QSqlDatabase &db, const QString &status)
{
    QSqlQuery query{db};
    query.prepare(QStringLiteral("SELECT EXISTS(SELECT 1 FROM content_status WHERE id = :status)"));
    query.bindValue(":status"_L1, status);
    return query.exec() && query.next() && query.value(0).toBool();
}

bool seriesExistsInDatabase(const QSqlDatabase &db, const QString &seriesId)
{
    if (seriesId.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query{db};
    query.prepare(QStringLiteral("SELECT EXISTS(SELECT 1 FROM series WHERE id = :id)"));
    query.bindValue(":id"_L1, seriesId);
    return query.exec() && query.next() && query.value(0).toBool();
}

bool seriesIsArchivedInDatabase(const QSqlDatabase &db, const QString &seriesId)
{
    if (seriesId.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query{db};
    query.prepare(QStringLiteral("SELECT status = 'archived' FROM series WHERE id = :id"));
    query.bindValue(":id"_L1, seriesId);
    return query.exec() && query.next() && query.value(0).toBool();
}

bool removeContentTree(QSqlDatabase &db, const QString &id, QString *errorMessage)
{
    QSqlQuery childQuery{db};
    childQuery.prepare(QStringLiteral("SELECT id FROM content WHERE parent_id = :parent_id"));
    childQuery.bindValue(":parent_id"_L1, id);
    if (!childQuery.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = childQuery.lastError().text();
        }
        return false;
    }

    while (childQuery.next()) {
        if (!removeContentTree(db, childQuery.value(0).toString(), errorMessage)) {
            return false;
        }
    }

    for (const auto &statement : {
             QStringLiteral("DELETE FROM publication WHERE content_id = :id"),
             QStringLiteral("DELETE FROM note WHERE content_id = :id"),
             QStringLiteral("DELETE FROM content WHERE id = :id"),
         }) {
        QSqlQuery query{db};
        query.prepare(statement);
        query.bindValue(":id"_L1, id);
        if (!query.exec()) {
            if (errorMessage != nullptr) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    return true;
}

QString selectSummaryBase()
{
    return QStringLiteral(
        "SELECT c.id, COALESCE(c.parent_id, ''), COALESCE(c.burst_template_key, ''), c.title, COALESCE(c.description, ''), "
        "COALESCE(c.tags_cache, ''), COALESCE(p.display_name, ''), COALESCE(s.name, ''), COALESCE(k.display_name, ''), COALESCE(o.display_name, ''), "
        "COALESCE(ch.display_name, ''), c.status, c.priority, c.series_position, c.scheduled_at, c.published_at, "
        "(SELECT MIN(pub.published_at) FROM publication pub WHERE pub.content_id = c.id AND pub.published_at IS NOT NULL) "
        "FROM content c "
        "LEFT JOIN pillar p ON p.id = c.pillar_id "
        "LEFT JOIN series s ON s.id = c.series_id "
        "LEFT JOIN content_kind k ON k.id = c.kind_id "
        "LEFT JOIN outcome o ON o.id = c.outcome_id "
        "LEFT JOIN channel ch ON ch.id = c.suggested_channel_id ");
}

} // namespace

ContentRepository::ContentRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::ContentSummary> ContentRepository::inboxItems(const QString &searchQuery) const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.status = 'inbox' "
                      "ORDER BY c.created_at DESC, c.priority DESC"));
    query.exec();
    return filteredItems(runSummaryQuery(query), searchQuery);
}

std::vector<Domain::ContentSummary> ContentRepository::boardItems(const QString &status, bool includeArchived, const QString &searchQuery) const
{
    if (!contentStatusExists(database_, status)) {
        return {};
    }

    QSqlQuery query{database_};
    if (status == "archived"_L1) {
        query.prepare(selectSummaryBase() + QStringLiteral(
                          "WHERE c.status = 'archived' AND :include_archived = 1 "
                          "ORDER BY c.updated_at DESC, c.priority DESC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    } else {
        query.prepare(selectSummaryBase() + QStringLiteral(
                          "WHERE c.status = :status "
                          "ORDER BY c.updated_at DESC, c.priority DESC"));
        query.bindValue(":status"_L1, status);
    }
    query.exec();
    return filteredItems(runSummaryQuery(query), searchQuery);
}

std::vector<Domain::ContentSummary> ContentRepository::rootItems(bool includeArchived, const QString &searchQuery) const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.parent_id IS NULL AND (:include_archived = 1 OR c.status != 'archived') "
                      "ORDER BY c.updated_at DESC, c.title ASC, c.priority DESC"));
    query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    query.exec();
    return filteredItems(runSummaryQuery(query), searchQuery);
}

std::vector<Domain::ContentSummary> ContentRepository::allItems(bool includeArchived, SortMode sortMode, const QString &searchQuery) const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE (:include_archived = 1 OR c.status != 'archived')"));
    query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    query.exec();

    auto results = filteredItems(runSummaryQuery(query), searchQuery);
    const auto compareText = [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    };
    const auto comparePriority = [](int left, int right) {
        return left > right;
    };
    const auto compareTextThenPriority = [&](const QString &leftText, const QString &rightText, int leftPriority, int rightPriority) {
        const auto textOrder = QString::compare(leftText, rightText, Qt::CaseInsensitive);
        if (textOrder != 0) {
            return textOrder < 0;
        }
        if (leftPriority != rightPriority) {
            return comparePriority(leftPriority, rightPriority);
        }
        return false;
    };
    const auto compareDate = [&](const QDateTime &left,
                                 const QDateTime &right,
                                 const QString &leftTitle,
                                 const QString &rightTitle,
                                 int leftPriority,
                                 int rightPriority) {
        const auto leftValid = left.isValid();
        const auto rightValid = right.isValid();
        if (leftValid != rightValid) {
            return leftValid;
        }
        if (leftValid && rightValid) {
            if (left != right) {
                return left < right;
            }
        }
        return compareTextThenPriority(leftTitle, rightTitle, leftPriority, rightPriority);
    };

    std::ranges::sort(results, [&](const auto &left, const auto &right) {
        switch (sortMode) {
        case SortMode::PriorityAlphabetical:
            if (left.priority != right.priority) {
                return comparePriority(left.priority, right.priority);
            }
            return compareText(left.title, right.title);
        case SortMode::Alphabetical:
            return compareTextThenPriority(left.title, right.title, left.priority, right.priority);
        case SortMode::StatusAlphabetical:
            return left.status == right.status
                ? compareTextThenPriority(left.title, right.title, left.priority, right.priority)
                : compareTextThenPriority(left.status, right.status, left.priority, right.priority);
        case SortMode::StatusDueDate:
            return left.status == right.status
                ? compareDate(left.scheduledAt, right.scheduledAt, left.title, right.title, left.priority, right.priority)
                : compareTextThenPriority(left.status, right.status, left.priority, right.priority);
        case SortMode::StatusFirstPublishDate: {
            const auto leftDate = left.firstPublicationAt.isValid() ? left.firstPublicationAt : left.publishedAt;
            const auto rightDate = right.firstPublicationAt.isValid() ? right.firstPublicationAt : right.publishedAt;
            return left.status == right.status
                ? compareDate(leftDate, rightDate, left.title, right.title, left.priority, right.priority)
                : compareTextThenPriority(left.status, right.status, left.priority, right.priority);
        }
        case SortMode::PillarAlphabetical:
            return left.pillarName == right.pillarName
                ? compareTextThenPriority(left.title, right.title, left.priority, right.priority)
                : compareTextThenPriority(left.pillarName, right.pillarName, left.priority, right.priority);
        case SortMode::PillarDueDate:
            return left.pillarName == right.pillarName
                ? compareDate(left.scheduledAt, right.scheduledAt, left.title, right.title, left.priority, right.priority)
                : compareTextThenPriority(left.pillarName, right.pillarName, left.priority, right.priority);
        case SortMode::PillarFirstPublishDate: {
            const auto leftDate = left.firstPublicationAt.isValid() ? left.firstPublicationAt : left.publishedAt;
            const auto rightDate = right.firstPublicationAt.isValid() ? right.firstPublicationAt : right.publishedAt;
            return left.pillarName == right.pillarName
                ? compareDate(leftDate, rightDate, left.title, right.title, left.priority, right.priority)
                : compareTextThenPriority(left.pillarName, right.pillarName, left.priority, right.priority);
        }
        case SortMode::DueDateAlphabetical:
        default:
            return compareDate(left.scheduledAt, right.scheduledAt, left.title, right.title, left.priority, right.priority);
        }
    });

    return results;
}

std::vector<Domain::ContentSummary> ContentRepository::childItems(const QString &parentId, const QString &searchQuery) const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.parent_id = :parent_id "
                      "ORDER BY c.created_at ASC, c.priority DESC"));
    query.bindValue(":parent_id"_L1, parentId);
    query.exec();
    return filteredItems(runSummaryQuery(query), searchQuery);
}

std::vector<Domain::ContentSummary> ContentRepository::listContentForSeries(const QString &seriesId, const QString &searchQuery) const
{
    if (!seriesExistsInDatabase(database_, seriesId)) {
        return {};
    }

    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.series_id = :series_id "
                      "ORDER BY c.series_position IS NULL, c.series_position ASC, "
                      "c.scheduled_at IS NULL, c.scheduled_at ASC, c.created_at ASC, c.priority DESC"));
    query.bindValue(":series_id"_L1, seriesId);
    query.exec();
    return filteredItems(runSummaryQuery(query), searchQuery);
}

std::vector<Domain::BurstTemplate> ContentRepository::activeBurstTemplates() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT key, display_name, title_suffix, kind_id, COALESCE(suggested_channel_id, ''), COALESCE(outcome_id, '') "
        "FROM burst_template "
        "WHERE is_active = 1 "
        "ORDER BY display_name ASC"));
    query.exec();

    std::vector<Domain::BurstTemplate> results;
    while (query.next()) {
        results.push_back({
            .key = query.value(0).toString(),
            .displayName = query.value(1).toString(),
            .titleSuffix = query.value(2).toString(),
            .kindId = query.value(3).toString(),
            .suggestedChannelId = query.value(4).toString(),
            .outcomeId = query.value(5).toString(),
        });
    }
    return results;
}

QString ContentRepository::create(const Domain::ContentItem &content, QString *errorMessage) const
{
    if (!contentStatusExists(database_, content.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid content status: %1").arg(content.status);
        }
        return {};
    }
    if (content.priority < 0 || content.priority > 100) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Priority out of range");
        }
        return {};
    }
    if (!content.seriesId.trimmed().isEmpty()) {
        if (!seriesExistsInDatabase(database_, content.seriesId)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Series not found");
            }
            return {};
        }
        if (seriesIsArchivedInDatabase(database_, content.seriesId)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Cannot assign content to an archived series.");
            }
            return {};
        }
    }

    const auto id = content.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : content.id;
    const auto createdAt = content.createdAt.isValid() ? content.createdAt : QDateTime::currentDateTimeUtc();
    const auto updatedAt = content.updatedAt.isValid() ? content.updatedAt : createdAt;
    const auto publishedAt = content.status == "published"_L1 && !content.publishedAt.isValid()
        ? updatedAt
        : content.publishedAt;
    const auto autoSeriesPosition = !content.seriesId.isEmpty() && content.parentId.isEmpty() && !content.hasSeriesPosition;
    const auto seriesPosition = content.hasSeriesPosition ? QVariant{content.seriesPosition}
                                                          : autoSeriesPosition ? QVariant{nextSeriesPosition(content.seriesId)}
                                                                               : QVariant{};

    QSqlQuery savepoint{database_};
    savepoint.exec(QStringLiteral("SAVEPOINT content_create"));

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO content "
        "(id, parent_id, series_id, series_position, burst_template_key, title, description, kind_id, pillar_id, outcome_id, suggested_channel_id, status, priority, scheduled_at, published_at, published_url, created_at, updated_at) "
        "VALUES "
        "(:id, :parent_id, :series_id, :series_position, :burst_template_key, :title, :description, :kind_id, :pillar_id, :outcome_id, :suggested_channel_id, :status, :priority, :scheduled_at, :published_at, :published_url, :created_at, :updated_at)"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":parent_id"_L1, nullableString(content.parentId));
    query.bindValue(":series_id"_L1, nullableString(content.seriesId));
    query.bindValue(":series_position"_L1, seriesPosition);
    query.bindValue(":burst_template_key"_L1, nullableString(content.burstTemplateKey));
    query.bindValue(":title"_L1, content.title);
    query.bindValue(":description"_L1, nullableString(content.description));
    query.bindValue(":kind_id"_L1, content.kindId);
    query.bindValue(":pillar_id"_L1, content.pillarId);
    query.bindValue(":outcome_id"_L1, nullableString(content.outcomeId));
    query.bindValue(":suggested_channel_id"_L1, nullableString(content.suggestedChannelId));
    query.bindValue(":status"_L1, content.status);
    query.bindValue(":priority"_L1, content.priority);
    query.bindValue(":scheduled_at"_L1, nullableDateTime(content.scheduledAt));
    query.bindValue(":published_at"_L1, nullableDateTime(publishedAt));
    query.bindValue(":published_url"_L1, nullableString(content.publishedUrl));
    query.bindValue(":created_at"_L1, createdAt.toString(Qt::ISODate));
    query.bindValue(":updated_at"_L1, updatedAt.toString(Qt::ISODate));
    if (!query.exec()) {
        QSqlQuery rollback{database_};
        rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT content_create"));
        rollback.exec(QStringLiteral("RELEASE SAVEPOINT content_create"));
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }

    if (!syncContentTags(database_, id, content.tags, errorMessage)) {
        QSqlQuery rollback{database_};
        rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT content_create"));
        rollback.exec(QStringLiteral("RELEASE SAVEPOINT content_create"));
        return {};
    }

    QSqlQuery release{database_};
    release.exec(QStringLiteral("RELEASE SAVEPOINT content_create"));

    return id;
}

QString ContentRepository::createInSeries(const QString &seriesId, Domain::ContentItem content, QString *errorMessage) const
{
    if (!seriesExistsInDatabase(database_, seriesId)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series not found");
        }
        return {};
    }
    if (seriesIsArchivedInDatabase(database_, seriesId)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot assign content to an archived series.");
        }
        return {};
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral("SELECT COALESCE(pillar_id, '') FROM series WHERE id = :id"));
    query.bindValue(":id"_L1, seriesId);
    query.exec();
    if (query.next() && content.pillarId.trimmed().isEmpty()) {
        content.pillarId = query.value(0).toString();
    }

    content.seriesId = seriesId;
    if (!content.hasSeriesPosition && content.parentId.trimmed().isEmpty()) {
        content.seriesPosition = nextSeriesPosition(seriesId);
        content.hasSeriesPosition = true;
    }
    return create(content, errorMessage);
}

bool ContentRepository::update(const Domain::ContentItem &content, QString *errorMessage) const
{
    if (content.id.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content id is required");
        }
        return false;
    }
    if (!contentStatusExists(database_, content.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid content status: %1").arg(content.status);
        }
        return false;
    }
    if (content.title.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Title is required");
        }
        return false;
    }
    if (content.priority < 0 || content.priority > 100) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Priority out of range");
        }
        return false;
    }
    if (!content.seriesId.trimmed().isEmpty()) {
        if (!seriesExistsInDatabase(database_, content.seriesId)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Series not found");
            }
            return false;
        }
        if (seriesIsArchivedInDatabase(database_, content.seriesId)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Cannot assign content to an archived series.");
            }
            return false;
        }
    }

    const auto existing = getContentById(content.id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    const auto updatedAt = QDateTime::currentDateTimeUtc();
    const auto publishedAt = content.status == "published"_L1 && !existing.publishedAt.isValid()
        ? updatedAt
        : existing.publishedAt;

    QSqlQuery savepoint{database_};
    savepoint.exec(QStringLiteral("SAVEPOINT content_update"));

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE content SET "
        "title = :title, "
        "description = :description, "
        "series_id = :series_id, "
        "series_position = :series_position, "
        "kind_id = :kind_id, "
        "pillar_id = :pillar_id, "
        "suggested_channel_id = :suggested_channel_id, "
        "status = :status, "
        "priority = :priority, "
        "scheduled_at = :scheduled_at, "
        "published_at = :published_at, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(":id"_L1, content.id);
    query.bindValue(":title"_L1, content.title.trimmed());
    query.bindValue(":description"_L1, nullableString(content.description.trimmed()));
    const auto seriesChanged = existing.seriesId != content.seriesId;
    const auto nextPositionForSeries = [&]() -> QVariant {
        if (content.seriesId.trimmed().isEmpty()) {
            return {};
        }
        if (content.parentId.trimmed().isEmpty()) {
            if (content.hasSeriesPosition) {
                return QVariant{content.seriesPosition};
            }
            if (seriesChanged || !existing.hasSeriesPosition) {
                return QVariant{nextSeriesPosition(content.seriesId)};
            }
        }
        return existing.hasSeriesPosition ? QVariant{existing.seriesPosition} : QVariant{};
    }();
    query.bindValue(":series_id"_L1, nullableString(content.seriesId.trimmed()));
    query.bindValue(":series_position"_L1, nextPositionForSeries);
    query.bindValue(":kind_id"_L1, content.kindId);
    query.bindValue(":pillar_id"_L1, content.pillarId);
    query.bindValue(":suggested_channel_id"_L1, nullableString(content.suggestedChannelId));
    query.bindValue(":status"_L1, content.status);
    query.bindValue(":priority"_L1, content.priority);
    query.bindValue(":scheduled_at"_L1, nullableDateTime(content.scheduledAt));
    query.bindValue(":published_at"_L1, nullableDateTime(publishedAt));
    query.bindValue(":updated_at"_L1, updatedAt.toString(Qt::ISODate));
    if (query.exec()) {
        if (!syncContentTags(database_, content.id, content.tags, errorMessage)) {
            QSqlQuery rollback{database_};
            rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT content_update"));
            rollback.exec(QStringLiteral("RELEASE SAVEPOINT content_update"));
            return false;
        }
        if (seriesChanged && !existing.seriesId.isEmpty() && !normalizeSeriesPositions(existing.seriesId, errorMessage)) {
            QSqlQuery rollback{database_};
            rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT content_update"));
            rollback.exec(QStringLiteral("RELEASE SAVEPOINT content_update"));
            return false;
        }
        if (!content.seriesId.isEmpty() && !normalizeSeriesPositions(content.seriesId, errorMessage)) {
            QSqlQuery rollback{database_};
            rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT content_update"));
            rollback.exec(QStringLiteral("RELEASE SAVEPOINT content_update"));
            return false;
        }
        QSqlQuery release{database_};
        release.exec(QStringLiteral("RELEASE SAVEPOINT content_update"));
        return true;
    }

    QSqlQuery rollback{database_};
    rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT content_update"));
    rollback.exec(QStringLiteral("RELEASE SAVEPOINT content_update"));
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool ContentRepository::updateStatus(const QString &id, const QString &newStatus, QString *errorMessage) const
{
    const auto existing = getContentById(id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    if (!contentStatusExists(database_, newStatus)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid content status: %1").arg(newStatus);
        }
        return false;
    }

    const auto now = QDateTime::currentDateTimeUtc();
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE content "
        "SET status = :status, "
        "updated_at = :updated_at, "
        "published_at = CASE "
        "    WHEN :status = 'published' AND published_at IS NULL THEN :updated_at "
        "    ELSE published_at "
        "END "
        "WHERE id = :id"));
    query.bindValue(":status"_L1, newStatus);
    query.bindValue(":updated_at"_L1, now.toString(Qt::ISODate));
    query.bindValue(":id"_L1, id);
    if (query.exec()) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool ContentRepository::assignContentToSeries(const QString &contentId, const QString &seriesId, QString *errorMessage) const
{
    auto content = getContentById(contentId);
    if (content.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }
    if (!seriesExistsInDatabase(database_, seriesId)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series not found");
        }
        return false;
    }
    if (seriesIsArchivedInDatabase(database_, seriesId)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot assign content to an archived series.");
        }
        return false;
    }

    content.seriesId = seriesId;
    content.hasSeriesPosition = false;
    return update(content, errorMessage);
}

bool ContentRepository::removeContentFromSeries(const QString &contentId, QString *errorMessage) const
{
    auto content = getContentById(contentId);
    if (content.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    content.seriesId.clear();
    content.seriesPosition = 0;
    content.hasSeriesPosition = false;
    return update(content, errorMessage);
}

bool ContentRepository::setSeriesPosition(const QString &contentId, int position, QString *errorMessage) const
{
    if (position < 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series position must be at least 1");
        }
        return false;
    }

    const auto content = getContentById(contentId);
    if (content.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }
    if (content.seriesId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content is not assigned to a series");
        }
        return false;
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE content SET series_position = :series_position, updated_at = :updated_at WHERE id = :id"));
    query.bindValue(":series_position"_L1, position);
    query.bindValue(":updated_at"_L1, nowIso());
    query.bindValue(":id"_L1, contentId);
    if (!execWithError(query, errorMessage)) {
        return false;
    }
    return normalizeSeriesPositions(content.seriesId, errorMessage);
}

bool ContentRepository::moveSeriesItem(const QString &seriesId, const QString &contentId, int direction, QString *errorMessage) const
{
    if (direction != -1 && direction != 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Direction must be -1 or 1");
        }
        return false;
    }

    const auto items = listContentForSeries(seriesId);
    const auto it = std::ranges::find_if(items, [&](const auto &item) { return item.id == contentId; });
    if (it == items.end()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series content item not found");
        }
        return false;
    }

    const auto index = static_cast<int>(std::distance(items.begin(), it));
    const auto targetIndex = index + direction;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(items.size())) {
        return true;
    }

    const auto currentId = items.at(index).id;
    const auto targetId = items.at(targetIndex).id;
    const auto currentPosition = items.at(index).hasSeriesPosition ? items.at(index).seriesPosition : index + 1;
    const auto targetPosition = items.at(targetIndex).hasSeriesPosition ? items.at(targetIndex).seriesPosition : targetIndex + 1;

    QSqlQuery savepoint{database_};
    savepoint.exec(QStringLiteral("SAVEPOINT move_series_item"));

    QSqlQuery first{database_};
    first.prepare(QStringLiteral(
        "UPDATE content SET series_position = :series_position, updated_at = :updated_at WHERE id = :id"));
    first.bindValue(":series_position"_L1, targetPosition);
    first.bindValue(":updated_at"_L1, nowIso());
    first.bindValue(":id"_L1, currentId);
    if (!execWithError(first, errorMessage)) {
        QSqlQuery rollback{database_};
        rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT move_series_item"));
        rollback.exec(QStringLiteral("RELEASE SAVEPOINT move_series_item"));
        return false;
    }

    QSqlQuery second{database_};
    second.prepare(QStringLiteral(
        "UPDATE content SET series_position = :series_position, updated_at = :updated_at WHERE id = :id"));
    second.bindValue(":series_position"_L1, currentPosition);
    second.bindValue(":updated_at"_L1, nowIso());
    second.bindValue(":id"_L1, targetId);
    if (!execWithError(second, errorMessage) || !normalizeSeriesPositions(seriesId, errorMessage)) {
        QSqlQuery rollback{database_};
        rollback.exec(QStringLiteral("ROLLBACK TO SAVEPOINT move_series_item"));
        rollback.exec(QStringLiteral("RELEASE SAVEPOINT move_series_item"));
        return false;
    }

    QSqlQuery release{database_};
    release.exec(QStringLiteral("RELEASE SAVEPOINT move_series_item"));
    return true;
}

bool ContentRepository::remove(const QString &id, QString *errorMessage) const
{
    if (id.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content id is required");
        }
        return false;
    }

    const auto existing = getContentById(id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    auto db = database_;
    if (!db.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    if (!removeContentTree(db, id, errorMessage)) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool ContentRepository::createBurst(const QString &sourceContentId, QString *errorMessage) const
{
    QStringList templateKeys;
    for (const auto &burstTemplate : activeBurstTemplates()) {
        templateKeys.append(burstTemplate.key);
    }
    return createBurst(sourceContentId, templateKeys, errorMessage);
}

bool ContentRepository::createBurst(const QString &sourceContentId,
                                    const QStringList &templateKeys,
                                    QString *errorMessage) const
{
    const auto source = getContentById(sourceContentId);
    if (source.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Source content not found");
        }
        return false;
    }
    if (!source.parentId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Only root content may generate bursts");
        }
        return false;
    }
    if (templateKeys.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Select at least one burst alternative");
        }
        return false;
    }

    auto db = database_;
    if (!db.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    QSqlQuery templateQuery{db};
    for (const auto &burstTemplateKey : templateKeys) {
        templateQuery.prepare(QStringLiteral(
            "SELECT bt.key, bt.title_suffix, bt.kind_id, bt.outcome_id, bt.suggested_channel_id "
            "FROM burst_template bt "
            "WHERE bt.key = :key AND bt.is_active = 1"));
        templateQuery.bindValue(":key"_L1, burstTemplateKey);
        if (!templateQuery.exec()) {
            if (errorMessage != nullptr) {
                *errorMessage = templateQuery.lastError().text();
            }
            db.rollback();
            return false;
        }
        if (!templateQuery.next()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Burst alternative not found: %1").arg(burstTemplateKey);
            }
            db.rollback();
            return false;
        }

        QSqlQuery existingQuery{db};
        existingQuery.prepare(QStringLiteral(
            "SELECT id FROM content "
            "WHERE parent_id = :parent_id AND burst_template_key = :burst_template_key"));
        existingQuery.bindValue(":parent_id"_L1, source.id);
        existingQuery.bindValue(":burst_template_key"_L1, burstTemplateKey);
        if (!existingQuery.exec()) {
            if (errorMessage != nullptr) {
                *errorMessage = existingQuery.lastError().text();
            }
            db.rollback();
            return false;
        }
        if (existingQuery.next()) {
            continue;
        }

        Domain::ContentItem child{
            .parentId = source.id,
            .seriesId = source.seriesId,
            .burstTemplateKey = burstTemplateKey,
            .title = source.title + templateQuery.value(1).toString(),
            .description = source.description,
            .tags = source.tags,
            .kindId = templateQuery.value(2).toString(),
            .pillarId = source.pillarId,
            .outcomeId = templateQuery.value(3).toString(),
            .suggestedChannelId = templateQuery.value(4).toString(),
            .status = QStringLiteral("shaping"),
            .priority = source.priority,
            .createdAt = QDateTime::currentDateTimeUtc(),
            .updatedAt = QDateTime::currentDateTimeUtc(),
        };
        if (create(child, errorMessage).isEmpty()) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

Domain::ContentItem ContentRepository::getById(const QString &id) const
{
    return getContentById(id);
}

std::vector<Domain::ContentSummary> ContentRepository::filteredItems(std::vector<Domain::ContentSummary> items,
                                                                     const QString &searchQuery) const
{
    const auto parsed = parseSearchQuery(searchQuery);
    if (parsed.textTerms.isEmpty() && parsed.tagTerms.isEmpty()) {
        return items;
    }

    std::vector<Domain::ContentSummary> results;
    for (const auto &item : items) {
        if (matchesSearch(item, parsed)) {
            results.push_back(item);
        }
    }
    return results;
}

std::vector<Domain::ContentSummary> ContentRepository::runSummaryQuery(QSqlQuery &query) const
{
    std::vector<Domain::ContentSummary> results;
    while (query.next()) {
        const auto seriesPositionValue = query.value(13);
        results.push_back({
            .id = query.value(0).toString(),
            .parentId = query.value(1).toString(),
            .burstTemplateKey = query.value(2).toString(),
            .title = query.value(3).toString(),
            .description = query.value(4).toString(),
            .tags = query.value(5).toString(),
            .pillarName = query.value(6).toString(),
            .seriesName = query.value(7).toString(),
            .kindName = query.value(8).toString(),
            .outcomeName = query.value(9).toString(),
            .suggestedChannelName = query.value(10).toString(),
            .status = query.value(11).toString(),
            .priority = query.value(12).toInt(),
            .seriesPosition = seriesPositionValue.toInt(),
            .hasSeriesPosition = !seriesPositionValue.isNull(),
            .scheduledAt = query.value(14).toDateTime(),
            .publishedAt = query.value(15).toDateTime(),
            .firstPublicationAt = query.value(16).toDateTime(),
        });
    }
    return results;
}

QStringList ContentRepository::contentTags(const QString &contentId) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT ct.tag_id FROM content_tag ct "
        "WHERE ct.content_id = :content_id "
        "ORDER BY ct.tag_id ASC"));
    query.bindValue(":content_id"_L1, contentId);
    query.exec();

    QStringList tags;
    while (query.next()) {
        tags.append(query.value(0).toString());
    }
    return tags;
}

Domain::ContentItem ContentRepository::getContentById(const QString &id) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, COALESCE(parent_id, ''), COALESCE(series_id, ''), series_position, COALESCE(burst_template_key, ''), title, "
        "COALESCE(description, ''), kind_id, pillar_id, COALESCE(outcome_id, ''), COALESCE(suggested_channel_id, ''), "
        "status, priority, scheduled_at, published_at, COALESCE(published_url, ''), created_at, updated_at "
        "FROM content WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    query.exec();
    if (!query.next()) {
        return {};
    }
    const auto seriesPositionValue = query.value(3);
    return {
        .id = query.value(0).toString(),
        .parentId = query.value(1).toString(),
        .seriesId = query.value(2).toString(),
        .seriesPosition = seriesPositionValue.toInt(),
        .hasSeriesPosition = !seriesPositionValue.isNull(),
        .burstTemplateKey = query.value(4).toString(),
        .title = query.value(5).toString(),
        .description = query.value(6).toString(),
        .tags = joinedTags(contentTags(id)),
        .kindId = query.value(7).toString(),
        .pillarId = query.value(8).toString(),
        .outcomeId = query.value(9).toString(),
        .suggestedChannelId = query.value(10).toString(),
        .status = query.value(11).toString(),
        .priority = query.value(12).toInt(),
        .scheduledAt = query.value(13).toDateTime(),
        .publishedAt = query.value(14).toDateTime(),
        .publishedUrl = query.value(15).toString(),
        .createdAt = query.value(16).toDateTime(),
        .updatedAt = query.value(17).toDateTime(),
    };
}

int ContentRepository::nextSeriesPosition(const QString &seriesId) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("SELECT COALESCE(MAX(series_position), 0) + 1 FROM content WHERE series_id = :series_id"));
    query.bindValue(":series_id"_L1, seriesId);
    query.exec();
    return query.next() ? query.value(0).toInt() : 1;
}

bool ContentRepository::seriesExists(const QString &seriesId) const
{
    return seriesExistsInDatabase(database_, seriesId);
}

bool ContentRepository::seriesIsArchived(const QString &seriesId) const
{
    return seriesIsArchivedInDatabase(database_, seriesId);
}

bool ContentRepository::normalizeSeriesPositions(const QString &seriesId, QString *errorMessage) const
{
    if (seriesId.trimmed().isEmpty()) {
        return true;
    }

    QSqlQuery select{database_};
    select.prepare(QStringLiteral(
        "SELECT id FROM content "
        "WHERE series_id = :series_id AND series_position IS NOT NULL "
        "ORDER BY series_position ASC, scheduled_at IS NULL, scheduled_at ASC, created_at ASC"));
    select.bindValue(":series_id"_L1, seriesId);
    if (!execWithError(select, errorMessage)) {
        return false;
    }

    int nextPosition = 1;
    while (select.next()) {
        QSqlQuery update{database_};
        update.prepare(QStringLiteral(
            "UPDATE content SET series_position = :series_position, updated_at = :updated_at WHERE id = :id"));
        update.bindValue(":series_position"_L1, nextPosition++);
        update.bindValue(":updated_at"_L1, nowIso());
        update.bindValue(":id"_L1, select.value(0).toString());
        if (!execWithError(update, errorMessage)) {
            return false;
        }
    }
    return true;
}

} // namespace SmTool::Data
