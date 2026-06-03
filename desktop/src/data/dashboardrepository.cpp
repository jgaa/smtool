#include "data/dashboardrepository.h"

#include <QSqlQuery>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {

DashboardRepository::DashboardRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::CalendarEntry> DashboardRepository::calendarEntries() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT pub.id, c.id, c.title, COALESCE(s.name, ''), COALESCE(ch.display_name, ''), 'publication', "
        "COALESCE(pub.scheduled_at, c.scheduled_at) "
        "FROM publication pub "
        "JOIN content c ON c.id = pub.content_id "
        "LEFT JOIN series s ON s.id = c.series_id "
        "JOIN channel ch ON ch.id = pub.channel_id "
        "WHERE COALESCE(pub.scheduled_at, c.scheduled_at) IS NOT NULL "
        "UNION ALL "
        "SELECT c.id, c.id, c.title, COALESCE(s.name, ''), COALESCE(ch.display_name, ''), 'content', c.scheduled_at "
        "FROM content c "
        "LEFT JOIN series s ON s.id = c.series_id "
        "LEFT JOIN channel ch ON ch.id = c.suggested_channel_id "
        "WHERE c.scheduled_at IS NOT NULL "
        "  AND NOT EXISTS (SELECT 1 FROM publication pub WHERE pub.content_id = c.id) "
        "ORDER BY 7 ASC, 3 ASC"));
    query.exec();

    std::vector<Domain::CalendarEntry> entries;
    while (query.next()) {
        entries.push_back({
            .id = query.value(0).toString(),
            .contentId = query.value(1).toString(),
            .title = query.value(2).toString(),
            .seriesName = query.value(3).toString(),
            .channelName = query.value(4).toString(),
            .sourceType = query.value(5).toString(),
            .scheduledAt = query.value(6).toDateTime(),
        });
    }
    return entries;
}

Domain::DashboardData DashboardRepository::dashboardData(bool includeArchived) const
{
    auto data = Domain::DashboardData{};

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT p.display_name, COUNT(c.id), '' "
            "FROM pillar p "
            "LEFT JOIN content c ON c.pillar_id = p.id AND (:include_archived = 1 OR c.status != 'archived') "
            "GROUP BY p.id, p.display_name "
            "ORDER BY COUNT(c.id) DESC, p.sort_order ASC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.byPillar = runMetricQuery(query);
    }

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT s.name, COUNT(c.id), s.status "
            "FROM series s "
            "LEFT JOIN content c ON c.series_id = s.id AND (:include_archived = 1 OR c.status != 'archived') "
            "WHERE (:include_archived = 1 OR s.status != 'archived') "
            "GROUP BY s.id, s.name, s.status "
            "ORDER BY COUNT(c.id) DESC, s.updated_at DESC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.bySeries = runMetricQuery(query);
    }

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT status, COUNT(*), '' "
            "FROM content "
            "WHERE (:include_archived = 1 OR status != 'archived') "
            "GROUP BY status "
            "ORDER BY COUNT(*) DESC, status ASC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.byStatus = runMetricQuery(query);
    }

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT title, 1, strftime('%Y-%m-%d', scheduled_at) "
            "FROM content "
            "WHERE scheduled_at BETWEEN datetime('now') AND datetime('now', '+14 day') "
            "  AND (:include_archived = 1 OR status != 'archived') "
            "ORDER BY scheduled_at ASC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.upcoming = runMetricQuery(query);
    }

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT title, 1, strftime('%Y-%m-%d', COALESCE(published_at, updated_at)) "
            "FROM content "
            "WHERE (published_at BETWEEN datetime('now', '-30 day') AND datetime('now') "
            "   OR (status = 'published' AND updated_at BETWEEN datetime('now', '-30 day') AND datetime('now'))) "
            "  AND (:include_archived = 1 OR status != 'archived') "
            "ORDER BY COALESCE(published_at, updated_at) DESC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.publishedContent = runMetricQuery(query);
    }

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT c.title || ' / ' || ch.display_name, 1, strftime('%Y-%m-%d', p.published_at) "
            "FROM publication p "
            "JOIN content c ON c.id = p.content_id "
            "JOIN channel ch ON ch.id = p.channel_id "
            "WHERE p.published_at BETWEEN datetime('now', '-30 day') AND datetime('now') "
            "  AND (:include_archived = 1 OR c.status != 'archived') "
            "ORDER BY p.published_at DESC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.publishedPublications = runMetricQuery(query);
    }

    {
        QSqlQuery query{database_};
        query.prepare(QStringLiteral(
            "SELECT p.display_name, 0, 'No published content in 30 days' "
            "FROM pillar p "
            "LEFT JOIN content c ON c.pillar_id = p.id "
            "  AND (c.published_at BETWEEN datetime('now', '-30 day') AND datetime('now') "
            "       OR (c.status = 'published' AND c.updated_at BETWEEN datetime('now', '-30 day') AND datetime('now'))) "
            "  AND (:include_archived = 1 OR c.status != 'archived') "
            "GROUP BY p.id, p.display_name "
            "HAVING COUNT(c.id) = 0 "
            "ORDER BY p.sort_order ASC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
        query.exec();
        data.zeroPublishedPillars = runMetricQuery(query);
    }

    return data;
}

std::vector<Domain::DashboardMetric> DashboardRepository::runMetricQuery(QSqlQuery &query) const
{
    std::vector<Domain::DashboardMetric> metrics;
    while (query.next()) {
        metrics.push_back({
            .label = query.value(0).toString(),
            .value = query.value(1).toInt(),
            .secondary = query.value(2).toString(),
        });
    }
    return metrics;
}

} // namespace SmTool::Data
