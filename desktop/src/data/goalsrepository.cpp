#include "data/goalsrepository.h"

#include "domain/constants.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
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

QVariant nullableString(const QString &value)
{
    return value.trimmed().isEmpty() ? QVariant{} : QVariant{value.trimmed()};
}

QVariant nullableInt(int value)
{
    return value > 0 ? QVariant{value} : QVariant{};
}

QString singularLabel(const QString &metricType)
{
    if (metricType == "publication_count"_L1) {
        return QStringLiteral("publication");
    }
    return QStringLiteral("content item");
}

QString pluralLabel(const QString &metricType, int value)
{
    if (metricType == "publication_count"_L1) {
        return value == 1 ? QStringLiteral("publication") : QStringLiteral("publications");
    }
    return value == 1 ? QStringLiteral("content item") : QStringLiteral("content items");
}

QString scopeTableName(const QString &scopeType)
{
    if (scopeType == "pillar"_L1) {
        return QStringLiteral("pillar");
    }
    if (scopeType == "channel"_L1) {
        return QStringLiteral("channel");
    }
    if (scopeType == "kind"_L1) {
        return QStringLiteral("content_kind");
    }
    if (scopeType == "series"_L1) {
        return QStringLiteral("series");
    }
    if (scopeType == "tag"_L1) {
        return QStringLiteral("tag");
    }
    return {};
}

} // namespace

GoalsRepository::GoalsRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::Goal> GoalsRepository::listGoals() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, name, goal_type, scope_type, COALESCE(scope_id, ''), metric_type, "
        "COALESCE(target_value, 0), COALESCE(period_type, ''), COALESCE(period_value, 0), enabled, "
        "created_at, updated_at "
        "FROM goals "
        "ORDER BY enabled DESC, updated_at DESC, name COLLATE NOCASE ASC"));
    query.exec();

    std::vector<Domain::Goal> results;
    while (query.next()) {
        Domain::Goal goal{
            .id = query.value(0).toString(),
            .name = query.value(1).toString(),
            .goalType = query.value(2).toString(),
            .scopeType = query.value(3).toString(),
            .scopeId = query.value(4).toString(),
            .metricType = query.value(5).toString(),
            .targetValue = query.value(6).toInt(),
            .periodType = query.value(7).toString(),
            .periodValue = query.value(8).toInt(),
            .enabled = query.value(9).toBool(),
            .createdAt = QDateTime::fromString(query.value(10).toString(), Qt::ISODate),
            .updatedAt = QDateTime::fromString(query.value(11).toString(), Qt::ISODate),
        };
        goal.scopeDisplayName = scopeDisplayName(goal.scopeType, goal.scopeId);
        goal.summaryText = buildSummary(goal);
        results.push_back(std::move(goal));
    }
    return results;
}

Domain::Goal GoalsRepository::getGoal(const QString &id) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, name, goal_type, scope_type, COALESCE(scope_id, ''), metric_type, "
        "COALESCE(target_value, 0), COALESCE(period_type, ''), COALESCE(period_value, 0), enabled, "
        "created_at, updated_at "
        "FROM goals WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    if (!query.exec() || !query.next()) {
        return {};
    }

    Domain::Goal goal{
        .id = query.value(0).toString(),
        .name = query.value(1).toString(),
        .goalType = query.value(2).toString(),
        .scopeType = query.value(3).toString(),
        .scopeId = query.value(4).toString(),
        .metricType = query.value(5).toString(),
        .targetValue = query.value(6).toInt(),
        .periodType = query.value(7).toString(),
        .periodValue = query.value(8).toInt(),
        .enabled = query.value(9).toBool(),
        .createdAt = QDateTime::fromString(query.value(10).toString(), Qt::ISODate),
        .updatedAt = QDateTime::fromString(query.value(11).toString(), Qt::ISODate),
    };
    goal.scopeDisplayName = scopeDisplayName(goal.scopeType, goal.scopeId);
    goal.summaryText = buildSummary(goal);
    return goal;
}

QString GoalsRepository::createGoal(const Domain::Goal &goal, QString *errorMessage) const
{
    const auto id = goal.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : goal.id;
    const auto createdAt = goal.createdAt.isValid() ? goal.createdAt.toString(Qt::ISODate) : nowIso();
    const auto updatedAt = goal.updatedAt.isValid() ? goal.updatedAt.toString(Qt::ISODate) : createdAt;

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO goals "
        "(id, name, goal_type, scope_type, scope_id, metric_type, target_value, period_type, period_value, enabled, created_at, updated_at) "
        "VALUES (:id, :name, :goal_type, :scope_type, :scope_id, :metric_type, :target_value, :period_type, :period_value, :enabled, :created_at, :updated_at)"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":name"_L1, goal.name.trimmed());
    query.bindValue(":goal_type"_L1, goal.goalType);
    query.bindValue(":scope_type"_L1, goal.scopeType);
    query.bindValue(":scope_id"_L1, nullableString(goal.scopeId));
    query.bindValue(":metric_type"_L1, goal.metricType);
    query.bindValue(":target_value"_L1, nullableInt(goal.targetValue));
    query.bindValue(":period_type"_L1, nullableString(goal.periodType));
    query.bindValue(":period_value"_L1, nullableInt(goal.periodValue));
    query.bindValue(":enabled"_L1, goal.enabled ? 1 : 0);
    query.bindValue(":created_at"_L1, createdAt);
    query.bindValue(":updated_at"_L1, updatedAt);
    return execWithError(query, errorMessage) ? id : QString{};
}

bool GoalsRepository::updateGoal(const Domain::Goal &goal, QString *errorMessage) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE goals SET "
        "name = :name, "
        "goal_type = :goal_type, "
        "scope_type = :scope_type, "
        "scope_id = :scope_id, "
        "metric_type = :metric_type, "
        "target_value = :target_value, "
        "period_type = :period_type, "
        "period_value = :period_value, "
        "enabled = :enabled, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(":id"_L1, goal.id);
    query.bindValue(":name"_L1, goal.name.trimmed());
    query.bindValue(":goal_type"_L1, goal.goalType);
    query.bindValue(":scope_type"_L1, goal.scopeType);
    query.bindValue(":scope_id"_L1, nullableString(goal.scopeId));
    query.bindValue(":metric_type"_L1, goal.metricType);
    query.bindValue(":target_value"_L1, nullableInt(goal.targetValue));
    query.bindValue(":period_type"_L1, nullableString(goal.periodType));
    query.bindValue(":period_value"_L1, nullableInt(goal.periodValue));
    query.bindValue(":enabled"_L1, goal.enabled ? 1 : 0);
    query.bindValue(":updated_at"_L1, nowIso());
    return execWithError(query, errorMessage);
}

bool GoalsRepository::deleteGoal(const QString &id, QString *errorMessage) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("DELETE FROM goals WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    return execWithError(query, errorMessage);
}

bool GoalsRepository::setGoalEnabled(const QString &id, bool enabled, QString *errorMessage) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE goals SET enabled = :enabled, updated_at = :updated_at WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":enabled"_L1, enabled ? 1 : 0);
    query.bindValue(":updated_at"_L1, nowIso());
    return execWithError(query, errorMessage);
}

std::vector<Domain::GoalBalanceItem> GoalsRepository::listBalanceItems(const QString &goalId) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, goal_id, scope_type, scope_id, weight, sort_order "
        "FROM goal_balance_items "
        "WHERE goal_id = :goal_id "
        "ORDER BY sort_order ASC, scope_id ASC"));
    query.bindValue(":goal_id"_L1, goalId);
    query.exec();

    std::vector<Domain::GoalBalanceItem> results;
    while (query.next()) {
        Domain::GoalBalanceItem item{
            .id = query.value(0).toString(),
            .goalId = query.value(1).toString(),
            .scopeType = query.value(2).toString(),
            .scopeId = query.value(3).toString(),
            .weight = query.value(4).toInt(),
            .sortOrder = query.value(5).toInt(),
        };
        item.scopeDisplayName = scopeDisplayName(item.scopeType, item.scopeId);
        results.push_back(std::move(item));
    }
    return results;
}

bool GoalsRepository::updateBalanceItems(const QString &goalId,
                                         const QString &scopeType,
                                         const std::vector<Domain::GoalBalanceItem> &items,
                                         QString *errorMessage) const
{
    QSqlQuery clearQuery{database_};
    clearQuery.prepare(QStringLiteral("DELETE FROM goal_balance_items WHERE goal_id = :goal_id"));
    clearQuery.bindValue(":goal_id"_L1, goalId);
    if (!execWithError(clearQuery, errorMessage)) {
        return false;
    }

    for (const auto &item : items) {
        QSqlQuery insertQuery{database_};
        insertQuery.prepare(QStringLiteral(
            "INSERT INTO goal_balance_items "
            "(id, goal_id, scope_type, scope_id, weight, sort_order) "
            "VALUES (:id, :goal_id, :scope_type, :scope_id, :weight, :sort_order)"));
        insertQuery.bindValue(":id"_L1,
                              item.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : item.id);
        insertQuery.bindValue(":goal_id"_L1, goalId);
        insertQuery.bindValue(":scope_type"_L1, scopeType);
        insertQuery.bindValue(":scope_id"_L1, item.scopeId);
        insertQuery.bindValue(":weight"_L1, item.weight);
        insertQuery.bindValue(":sort_order"_L1, item.sortOrder);
        if (!execWithError(insertQuery, errorMessage)) {
            return false;
        }
    }

    return true;
}

bool GoalsRepository::scopeExists(const QString &scopeType, const QString &scopeId) const
{
    const auto tableName = scopeTableName(scopeType);
    if (tableName.isEmpty() || scopeId.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query{database_};
    if (scopeType == "tag"_L1) {
        query.prepare(QStringLiteral("SELECT EXISTS(SELECT 1 FROM tag WHERE id = :id)"));
        query.bindValue(":id"_L1, scopeId);
    } else if (scopeType == "series"_L1) {
        query.prepare(QStringLiteral("SELECT EXISTS(SELECT 1 FROM series WHERE id = :id)"));
        query.bindValue(":id"_L1, scopeId);
    } else {
        query.prepare(QStringLiteral("SELECT EXISTS(SELECT 1 FROM %1 WHERE id = :id)").arg(tableName));
        query.bindValue(":id"_L1, scopeId);
    }
    return query.exec() && query.next() && query.value(0).toBool();
}

QString GoalsRepository::scopeDisplayName(const QString &scopeType, const QString &scopeId) const
{
    if (scopeId.trimmed().isEmpty()) {
        return {};
    }

    QSqlQuery query{database_};
    if (scopeType == "tag"_L1) {
        query.prepare(QStringLiteral("SELECT id FROM tag WHERE id = :id"));
        query.bindValue(":id"_L1, scopeId);
        if (query.exec() && query.next()) {
            return QStringLiteral("#%1").arg(query.value(0).toString());
        }
        return scopeId;
    }

    if (scopeType == "series"_L1) {
        query.prepare(QStringLiteral("SELECT name FROM series WHERE id = :id"));
        query.bindValue(":id"_L1, scopeId);
        if (query.exec() && query.next()) {
            return query.value(0).toString();
        }
        return scopeId;
    }

    const auto tableName = scopeTableName(scopeType);
    if (tableName.isEmpty()) {
        return scopeId;
    }
    query.prepare(QStringLiteral("SELECT display_name FROM %1 WHERE id = :id").arg(tableName));
    query.bindValue(":id"_L1, scopeId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return scopeId;
}

QString GoalsRepository::metricTypeForScope(const QString &scopeType) const
{
    return scopeType == "channel"_L1 ? QStringLiteral("publication_count")
                                     : QStringLiteral("content_count");
}

QString GoalsRepository::periodLabel(const QString &periodType, int periodValue) const
{
    if (periodType == "rolling_days"_L1) {
        return QStringLiteral("%1 days").arg(periodValue);
    }
    if (periodValue > 1) {
        return QStringLiteral("%1 %2s").arg(periodValue).arg(periodType);
    }
    return periodType;
}

QString GoalsRepository::buildSummary(const Domain::Goal &goal) const
{
    if (goal.goalType == "balance"_L1) {
        const auto items = listBalanceItems(goal.id);
        int totalWeight = 0;
        for (const auto &item : items) {
            if (item.weight > 0) {
                totalWeight += item.weight;
            }
        }

        QStringList parts;
        for (const auto &item : items) {
            if (item.weight <= 0 || totalWeight <= 0) {
                continue;
            }
            const auto percentage = qRound((static_cast<double>(item.weight) * 100.0) / static_cast<double>(totalWeight));
            parts.append(QStringLiteral("%1 %2%").arg(item.scopeDisplayName).arg(percentage));
        }

        return parts.isEmpty() ? goal.name : QStringLiteral("%1: %2").arg(goal.name, parts.join(QStringLiteral(", ")));
    }

    const auto displayName = goal.scopeDisplayName.isEmpty() ? goal.name : goal.scopeDisplayName;
    const auto metricLabel = pluralLabel(goal.metricType, goal.targetValue);
    const auto period = periodLabel(goal.periodType, goal.periodValue);
    const auto qualifier = goal.goalType == "cadence"_L1 ? QStringLiteral("every") : QStringLiteral("per");
    return QStringLiteral("%1: at least %2 %3 %4 %5")
        .arg(displayName)
        .arg(goal.targetValue)
        .arg(metricLabel)
        .arg(qualifier)
        .arg(period);
}

} // namespace SmTool::Data
