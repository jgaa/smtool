#include "data/lookupsrepository.h"

#include <QSqlQuery>

namespace SmTool::Data {

LookupsRepository::LookupsRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::LookupValue> LookupsRepository::activeLookups(const QString &tableName) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, key, display_name, COALESCE(description, ''), sort_order, is_active "
        "FROM %1 "
        "ORDER BY is_active DESC, sort_order ASC, display_name ASC")
                      .arg(tableName));
    query.exec();

    std::vector<Domain::LookupValue> results;
    while (query.next()) {
        results.push_back({
            .id = query.value(0).toString(),
            .key = query.value(1).toString(),
            .displayName = query.value(2).toString(),
            .description = query.value(3).toString(),
            .sortOrder = query.value(4).toInt(),
            .isActive = query.value(5).toBool(),
        });
    }
    return results;
}

std::vector<Domain::ContentStatus> LookupsRepository::contentStatuses() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, COALESCE(info, ''), sort_order, is_system "
        "FROM content_status "
        "ORDER BY sort_order ASC, id ASC"));
    query.exec();

    std::vector<Domain::ContentStatus> results;
    while (query.next()) {
        results.push_back({
            .id = query.value(0).toString(),
            .info = query.value(1).toString(),
            .sortOrder = query.value(2).toInt(),
            .isSystem = query.value(3).toBool(),
        });
    }
    return results;
}

QString LookupsRepository::lookupIdByKey(const QString &tableName, const QString &key) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("SELECT id FROM %1 WHERE key = :key").arg(tableName));
    query.bindValue(QStringLiteral(":key"), key);
    query.exec();
    return query.next() ? query.value(0).toString() : QString{};
}

} // namespace SmTool::Data
