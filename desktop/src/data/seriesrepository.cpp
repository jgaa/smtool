#include "data/seriesrepository.h"

#include "domain/constants.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

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

} // namespace

SeriesRepository::SeriesRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::SeriesSummary> SeriesRepository::list(bool includeArchived, const QString &searchQuery) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT s.id, s.name, COALESCE(s.description, ''), COALESCE(p.display_name, ''), s.status, "
        "COUNT(c.id) AS content_count, "
        "SUM(CASE WHEN c.status = 'scheduled' THEN 1 ELSE 0 END) AS scheduled_count "
        "FROM series s "
        "LEFT JOIN pillar p ON p.id = s.pillar_id "
        "LEFT JOIN content c ON c.series_id = s.id "
        "WHERE (:include_archived = 1 OR s.status != 'archived') "
        "GROUP BY s.id, s.name, s.description, p.display_name, s.status "
        "ORDER BY s.updated_at DESC, s.name ASC"));
    query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    query.exec();

    const auto parsed = parseSearchQuery(searchQuery);
    std::vector<Domain::SeriesSummary> results;
    while (query.next()) {
        Domain::SeriesSummary item{
            .id = query.value(0).toString(),
            .name = query.value(1).toString(),
            .description = query.value(2).toString(),
            .pillarName = query.value(3).toString(),
            .status = query.value(4).toString(),
            .contentCount = query.value(5).toInt(),
            .scheduledCount = query.value(6).toInt(),
        };

        if (!parsed.tagTerms.isEmpty()) {
            continue;
        }
        const auto matches = parsed.scope == SearchTerms::Scope::TitleOnly
            ? containsAllTerms(item.name, parsed.textTerms)
            : parsed.scope == SearchTerms::Scope::DescriptionOnly
                ? containsAllTerms(item.description, parsed.textTerms)
                : containsAllTerms(item.name + u' ' + item.description, parsed.textTerms);
        if (parsed.textTerms.isEmpty() || matches) {
            results.push_back(std::move(item));
        }
    }
    return results;
}

Domain::SeriesDetail SeriesRepository::getById(const QString &id) const
{
    if (id.trimmed().isEmpty()) {
        return {};
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT s.id, s.name, COALESCE(s.description, ''), COALESCE(s.pillar_id, ''), "
        "COALESCE(p.display_name, ''), s.status, COUNT(c.id), "
        "SUM(CASE WHEN c.status = 'scheduled' THEN 1 ELSE 0 END), "
        "s.created_at, s.updated_at "
        "FROM series s "
        "LEFT JOIN pillar p ON p.id = s.pillar_id "
        "LEFT JOIN content c ON c.series_id = s.id "
        "WHERE s.id = :id "
        "GROUP BY s.id, s.name, s.description, s.pillar_id, p.display_name, s.status, s.created_at, s.updated_at"));
    query.bindValue(":id"_L1, id);
    query.exec();
    if (!query.next()) {
        return {};
    }

    return {
        .id = query.value(0).toString(),
        .name = query.value(1).toString(),
        .description = query.value(2).toString(),
        .pillarId = query.value(3).toString(),
        .pillarName = query.value(4).toString(),
        .status = query.value(5).toString(),
        .contentCount = query.value(6).toInt(),
        .scheduledCount = query.value(7).toInt(),
        .createdAt = query.value(8).toDateTime(),
        .updatedAt = query.value(9).toDateTime(),
    };
}

QString SeriesRepository::create(const QString &name,
                                 const QString &description,
                                 const QString &pillarId,
                                 const QString &status,
                                 QString *errorMessage) const
{
    if (!Domain::isValidSeriesStatus(status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid series status: %1").arg(status);
        }
        return {};
    }

    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO series (id, name, description, pillar_id, status, created_at, updated_at) "
        "VALUES (:id, :name, :description, :pillar_id, :status, :created_at, :updated_at)"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":name"_L1, name);
    query.bindValue(":description"_L1, description);
    query.bindValue(":pillar_id"_L1, pillarId.isEmpty() ? QVariant{} : QVariant{pillarId});
    query.bindValue(":status"_L1, status);
    query.bindValue(":created_at"_L1, now);
    query.bindValue(":updated_at"_L1, now);
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }
    return id;
}

bool SeriesRepository::update(const Domain::SeriesDetail &series, QString *errorMessage) const
{
    if (series.id.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series id is required");
        }
        return false;
    }
    if (series.name.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series name is required");
        }
        return false;
    }
    if (!Domain::isValidSeriesStatus(series.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid series status: %1").arg(series.status);
        }
        return false;
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE series SET "
        "name = :name, "
        "description = :description, "
        "pillar_id = :pillar_id, "
        "status = :status, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(":id"_L1, series.id);
    query.bindValue(":name"_L1, series.name.trimmed());
    query.bindValue(":description"_L1, series.description.trimmed());
    query.bindValue(":pillar_id"_L1, series.pillarId.trimmed().isEmpty() ? QVariant{} : QVariant{series.pillarId.trimmed()});
    query.bindValue(":status"_L1, series.status);
    query.bindValue(":updated_at"_L1, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return execWithError(query, errorMessage);
}

bool SeriesRepository::archive(const QString &id, QString *errorMessage) const
{
    const auto existing = getById(id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series not found");
        }
        return false;
    }

    auto updated = existing;
    updated.status = QStringLiteral("archived");
    return update(updated, errorMessage);
}

bool SeriesRepository::remove(const QString &id, QString *errorMessage) const
{
    const auto existing = getById(id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series not found");
        }
        return false;
    }
    if (existing.contentCount > 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Series has content. Archive it instead of deleting it.");
        }
        return false;
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral("DELETE FROM series WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    return execWithError(query, errorMessage);
}

} // namespace SmTool::Data
