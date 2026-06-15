#include "data/calendarrepository.h"

#include <QDate>
#include <QRegularExpression>
#include <QSqlQuery>

#include <ranges>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

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
            parsed.tagTerms.append(token.mid(1).toLower());
        } else {
            parsed.textTerms.append(token.toLower());
        }
    }
    return parsed;
}

bool containsAllTerms(const QString &haystack, const QStringList &terms)
{
    const auto lowered = haystack.toLower();
    return std::ranges::all_of(terms, [&](const auto &term) { return lowered.contains(term); });
}

} // namespace

CalendarRepository::CalendarRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::CalendarEntry> CalendarRepository::calendarEntries(bool includeArchived,
                                                                       bool includePublished,
                                                                       const QString &searchQuery) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT pub.id, c.id, c.title, COALESCE(s.name, ''), COALESCE(ch.display_name, ''), 'publication', "
        "c.status, pub.status, pub.scheduled_at, COALESCE(c.description, ''), COALESCE(c.tags_cache, '') "
        "FROM publication pub "
        "JOIN content c ON c.id = pub.content_id "
        "LEFT JOIN series s ON s.id = c.series_id "
        "JOIN channel ch ON ch.id = pub.channel_id "
        "WHERE pub.scheduled_at IS NOT NULL "
        "  AND (:include_archived = 1 OR c.status != 'archived') "
        "  AND (:include_published = 1 OR (COALESCE(pub.status, '') != 'published' AND pub.published_at IS NULL)) "
        "UNION ALL "
        "SELECT c.id, c.id, c.title, COALESCE(s.name, ''), COALESCE(ch.display_name, ''), 'content', c.status, '', c.scheduled_at, COALESCE(c.description, ''), COALESCE(c.tags_cache, '') "
        "FROM content c "
        "LEFT JOIN series s ON s.id = c.series_id "
        "LEFT JOIN channel ch ON ch.id = c.suggested_channel_id "
        "WHERE c.scheduled_at IS NOT NULL "
        "  AND (:include_archived = 1 OR c.status != 'archived') "
        "  AND (:include_published = 1 OR c.status != 'published') "
        "ORDER BY 9 ASC, 3 ASC"));
    query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    query.bindValue(":include_published"_L1, includePublished ? 1 : 0);
    query.exec();

    const auto parsed = parseSearchQuery(searchQuery);
    std::vector<Domain::CalendarEntry> entries;
    const auto today = QDate::currentDate();
    while (query.next()) {
        const auto title = query.value(2).toString();
        const auto description = query.value(9).toString();
        const auto tagsCache = query.value(10).toString();
        if (!std::ranges::all_of(parsed.tagTerms, [&](const auto &tag) {
                return tagsCache.split(u' ', Qt::SkipEmptyParts).contains(tag);
            })) {
            continue;
        }
        const auto matchesText = parsed.scope == SearchTerms::Scope::TitleOnly
            ? containsAllTerms(title, parsed.textTerms)
            : parsed.scope == SearchTerms::Scope::DescriptionOnly
                ? containsAllTerms(description, parsed.textTerms)
                : containsAllTerms(title + u' ' + description, parsed.textTerms);
        if (!parsed.textTerms.isEmpty() && !matchesText) {
            continue;
        }

        const auto contentStatus = query.value(6).toString();
        const auto publicationStatus = query.value(7).toString();
        const auto scheduledAt = query.value(8).toDateTime();
        const bool isPublicationEntry = query.value(5).toString() == "publication"_L1;
        const bool isComplete = contentStatus == "archived"_L1
            || (isPublicationEntry ? publicationStatus == "published"_L1 : contentStatus == "published"_L1);
        entries.push_back({
            .id = query.value(0).toString(),
            .contentId = query.value(1).toString(),
            .title = title,
            .seriesName = query.value(3).toString(),
            .channelName = query.value(4).toString(),
            .sourceType = query.value(5).toString(),
            .contentStatus = contentStatus,
            .publicationStatus = publicationStatus,
            .scheduledAt = scheduledAt,
            .isOverdue = scheduledAt.isValid() && scheduledAt.date() < today && !isComplete,
        });
    }
    return entries;
}

} // namespace SmTool::Data
