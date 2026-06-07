#include "app/dashboardservice.h"

#include <QDate>
#include <QDateTime>
#include <QSqlQuery>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <map>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::App {
namespace {

enum class CountMode {
    PublishedPerformance,
    PipelineEligible,
    ScheduledPipeline,
};

struct FocusAccumulator {
    Domain::DashboardRow row;
    bool initialized = false;
};

Domain::DashboardRow makeStatisticRow(const QString &displayName, const double value, const QString &detailText = {})
{
    return Domain::DashboardRow{
        .displayName = displayName,
        .detailText = detailText,
        .actualValue = value,
        .health = Domain::DashboardHealth::Unknown,
    };
}

QDate parseDateOrDefault(const QString &value, const QDate &fallback)
{
    const auto parsed = QDate::fromString(value.trimmed(), Qt::ISODate);
    return parsed.isValid() ? parsed : fallback;
}

QDateTime startOfDayUtc(const QDate &date)
{
    return QDateTime{date, QTime{0, 0}, QTimeZone::UTC};
}

QDateTime endExclusiveUtc(const QDate &date)
{
    return QDateTime{date.addDays(1), QTime{0, 0}, QTimeZone::UTC};
}

double roundOneDecimal(const double value)
{
    return std::round(value * 10.0) / 10.0;
}

int healthRank(const Domain::DashboardHealth health)
{
    switch (health) {
    case Domain::DashboardHealth::Critical:
        return 0;
    case Domain::DashboardHealth::Bad:
        return 1;
    case Domain::DashboardHealth::Warning:
        return 2;
    case Domain::DashboardHealth::SlightlyLow:
        return 3;
    case Domain::DashboardHealth::Good:
        return 4;
    case Domain::DashboardHealth::TooHigh:
        return 5;
    case Domain::DashboardHealth::Unknown:
    default:
        return 6;
    }
}

Domain::DashboardPeriod defaultPerformancePeriod()
{
    const auto today = QDate::currentDate();
    return {
        .key = QStringLiteral("last_90_days"),
        .label = QStringLiteral("Last 90 days"),
        .startDate = today.addDays(-89),
        .endDate = today,
    };
}

Domain::DashboardPeriod defaultPipelinePeriod()
{
    const auto today = QDate::currentDate();
    return {
        .key = QStringLiteral("next_30_days"),
        .label = QStringLiteral("Next 30 days"),
        .startDate = today,
        .endDate = today.addDays(29),
    };
}

Domain::DashboardPeriod resolvePerformancePeriod(const DashboardService::PeriodSelection &selection)
{
    const auto today = QDate::currentDate();
    if (selection.key == "last_30_days"_L1) {
        return {selection.key, QStringLiteral("Last 30 days"), today.addDays(-29), today};
    }
    if (selection.key == "this_quarter"_L1) {
        const auto quarterStartMonth = ((today.month() - 1) / 3) * 3 + 1;
        const auto startDate = QDate{today.year(), quarterStartMonth, 1};
        return {selection.key, QStringLiteral("This quarter"), startDate, startDate.addMonths(3).addDays(-1)};
    }
    if (selection.key == "this_year"_L1) {
        return {selection.key, QStringLiteral("This year"), QDate{today.year(), 1, 1}, QDate{today.year(), 12, 31}};
    }
    if (selection.key == "custom"_L1) {
        const auto fallback = defaultPerformancePeriod();
        auto startDate = parseDateOrDefault(selection.startDate, fallback.startDate);
        auto endDate = parseDateOrDefault(selection.endDate, fallback.endDate);
        if (endDate < startDate) {
            std::swap(startDate, endDate);
        }
        return {selection.key, QStringLiteral("Custom"), startDate, endDate};
    }
    return defaultPerformancePeriod();
}

Domain::DashboardPeriod resolvePipelinePeriod(const DashboardService::PeriodSelection &selection)
{
    const auto today = QDate::currentDate();
    if (selection.key == "next_7_days"_L1) {
        return {selection.key, QStringLiteral("Next 7 days"), today, today.addDays(6)};
    }
    if (selection.key == "next_90_days"_L1) {
        return {selection.key, QStringLiteral("Next 90 days"), today, today.addDays(89)};
    }
    if (selection.key == "custom"_L1) {
        const auto fallback = defaultPipelinePeriod();
        auto startDate = parseDateOrDefault(selection.startDate, fallback.startDate);
        auto endDate = parseDateOrDefault(selection.endDate, fallback.endDate);
        if (endDate < startDate) {
            std::swap(startDate, endDate);
        }
        return {selection.key, QStringLiteral("Custom"), startDate, endDate};
    }
    return defaultPipelinePeriod();
}

double approximatePeriodCount(const Domain::Goal &goal, const Domain::DashboardPeriod &period)
{
    const auto days = std::max<qint64>(1, period.startDate.daysTo(period.endDate.addDays(1)));

    double baseDays = 1.0;
    if (goal.periodType == "week"_L1) {
        baseDays = 7.0;
    } else if (goal.periodType == "month"_L1) {
        baseDays = 30.0;
    } else if (goal.periodType == "quarter"_L1) {
        baseDays = 91.0;
    } else if (goal.periodType == "year"_L1) {
        baseDays = 365.0;
    } else if (goal.periodType == "rolling_days"_L1) {
        baseDays = static_cast<double>(std::max(goal.periodValue, 1));
    }

    if (goal.periodType != "rolling_days"_L1) {
        baseDays *= static_cast<double>(std::max(goal.periodValue, 1));
    }

    return std::max(1.0, std::ceil(static_cast<double>(days) / baseDays));
}

Domain::DashboardHealth classifyGoalHealth(const double percent)
{
    if (percent >= 150.0) {
        return Domain::DashboardHealth::TooHigh;
    }
    if (percent >= 100.0) {
        return Domain::DashboardHealth::Good;
    }
    if (percent >= 90.0) {
        return Domain::DashboardHealth::SlightlyLow;
    }
    if (percent >= 75.0) {
        return Domain::DashboardHealth::Warning;
    }
    if (percent >= 50.0) {
        return Domain::DashboardHealth::Bad;
    }
    return Domain::DashboardHealth::Critical;
}

Domain::DashboardHealth classifyPipelineHealth(const double percent)
{
    if (percent >= 150.0) {
        return Domain::DashboardHealth::TooHigh;
    }
    if (percent >= 100.0) {
        return Domain::DashboardHealth::Good;
    }
    if (percent >= 75.0) {
        return Domain::DashboardHealth::Warning;
    }
    if (percent >= 50.0) {
        return Domain::DashboardHealth::Bad;
    }
    return Domain::DashboardHealth::Critical;
}

Domain::DashboardHealth classifyBalanceHealth(const double absoluteDeviation)
{
    if (absoluteDeviation <= 10.0) {
        return Domain::DashboardHealth::Good;
    }
    if (absoluteDeviation <= 20.0) {
        return Domain::DashboardHealth::Warning;
    }
    if (absoluteDeviation <= 35.0) {
        return Domain::DashboardHealth::Bad;
    }
    return Domain::DashboardHealth::Critical;
}

QString detailRangeLabel(const Domain::DashboardPeriod &period)
{
    return period.key == "custom"_L1
        ? QStringLiteral("%1 to %2").arg(period.startDate.toString(Qt::ISODate), period.endDate.toString(Qt::ISODate))
        : period.label;
}

QString contentScopeClause(const QString &scopeType, const QString &columnPrefix)
{
    if (scopeType == "pillar"_L1) {
        return QStringLiteral("%1pillar_id = :scope_id").arg(columnPrefix);
    }
    if (scopeType == "series"_L1) {
        return QStringLiteral("%1series_id = :scope_id").arg(columnPrefix);
    }
    if (scopeType == "kind"_L1) {
        return QStringLiteral("%1kind_id = :scope_id").arg(columnPrefix);
    }
    if (scopeType == "tag"_L1) {
        return QStringLiteral("EXISTS (SELECT 1 FROM content_tag ct WHERE ct.content_id = %1id AND ct.tag_id = :scope_id)")
            .arg(columnPrefix);
    }
    return {};
}

double countContent(const QSqlDatabase &database,
                    const QString &scopeType,
                    const QString &scopeId,
                    const CountMode mode,
                    const Domain::DashboardPeriod &period)
{
    const auto scopeClause = contentScopeClause(scopeType, QStringLiteral("c."));
    if (scopeClause.isEmpty()) {
        return 0.0;
    }

    QString sql = QStringLiteral("SELECT COUNT(*) FROM content c WHERE %1 ").arg(scopeClause);
    if (mode == CountMode::PublishedPerformance) {
        sql += QStringLiteral("AND c.status = 'published' "
                              "AND COALESCE(c.published_at, c.updated_at) >= :start_at "
                              "AND COALESCE(c.published_at, c.updated_at) < :end_at ");
    } else {
        sql += QStringLiteral("AND c.status IN ('drafting', 'ready', 'scheduled') ");
    }

    QSqlQuery query{database};
    query.prepare(sql);
    query.bindValue(":scope_id"_L1, scopeId);
    if (mode == CountMode::PublishedPerformance) {
        query.bindValue(":start_at"_L1, startOfDayUtc(period.startDate).toString(Qt::ISODate));
        query.bindValue(":end_at"_L1, endExclusiveUtc(period.endDate).toString(Qt::ISODate));
    }
    return query.exec() && query.next() ? static_cast<double>(query.value(0).toInt()) : 0.0;
}

double countPublications(const QSqlDatabase &database,
                         const QString &scopeId,
                         const CountMode mode,
                         const Domain::DashboardPeriod &period)
{
    QString sql = QStringLiteral("SELECT COUNT(*) FROM publication p WHERE p.channel_id = :scope_id ");
    if (mode == CountMode::PublishedPerformance) {
        sql += QStringLiteral("AND p.status = 'published' "
                              "AND COALESCE(p.published_at, p.updated_at) >= :start_at "
                              "AND COALESCE(p.published_at, p.updated_at) < :end_at ");
    } else {
        sql += QStringLiteral("AND p.scheduled_at >= :start_at "
                              "AND p.scheduled_at < :end_at "
                              "AND COALESCE(p.status, '') != 'published' "
                              "AND p.published_at IS NULL ");
    }

    QSqlQuery query{database};
    query.prepare(sql);
    query.bindValue(":scope_id"_L1, scopeId);
    query.bindValue(":start_at"_L1, startOfDayUtc(period.startDate).toString(Qt::ISODate));
    query.bindValue(":end_at"_L1, endExclusiveUtc(period.endDate).toString(Qt::ISODate));
    return query.exec() && query.next() ? static_cast<double>(query.value(0).toInt()) : 0.0;
}

double countActualForGoal(const QSqlDatabase &database, const Domain::Goal &goal, const Domain::DashboardPeriod &period)
{
    if (goal.metricType == "publication_count"_L1) {
        return countPublications(database, goal.scopeId, CountMode::PublishedPerformance, period);
    }
    return countContent(database, goal.scopeType, goal.scopeId, CountMode::PublishedPerformance, period);
}

double countPipelineForGoal(const QSqlDatabase &database, const Domain::Goal &goal, const Domain::DashboardPeriod &period)
{
    if (goal.metricType == "publication_count"_L1) {
        return countPublications(database, goal.scopeId, CountMode::ScheduledPipeline, period);
    }
    return countContent(database, goal.scopeType, goal.scopeId, CountMode::PipelineEligible, period);
}

QDateTime latestPublishedAt(const QSqlDatabase &database, const Domain::Goal &goal)
{
    QSqlQuery query{database};
    if (goal.metricType == "publication_count"_L1) {
        query.prepare(QStringLiteral(
            "SELECT MAX(COALESCE(p.published_at, p.updated_at)) "
            "FROM publication p "
            "WHERE p.channel_id = :scope_id AND p.status = 'published'"));
        query.bindValue(":scope_id"_L1, goal.scopeId);
    } else {
        const auto scopeClause = contentScopeClause(goal.scopeType, QStringLiteral("c."));
        if (scopeClause.isEmpty()) {
            return {};
        }
        query.prepare(QStringLiteral(
            "SELECT MAX(COALESCE(c.published_at, c.updated_at)) "
            "FROM content c "
            "WHERE c.status = 'published' AND %1").arg(scopeClause));
        query.bindValue(":scope_id"_L1, goal.scopeId);
    }

    return query.exec() && query.next() ? query.value(0).toDateTime() : QDateTime{};
}

std::vector<Domain::DashboardRow> collectStatistics(const QSqlDatabase &database)
{
    std::vector<Domain::DashboardRow> rows;

    QSqlQuery statusQuery{database};
    statusQuery.prepare(QStringLiteral(
        "SELECT cs.id, COUNT(c.id) "
        "FROM content_status cs "
        "LEFT JOIN content c ON c.status = cs.id "
        "GROUP BY cs.id, cs.sort_order "
        "ORDER BY cs.sort_order ASC, cs.id ASC"));
    if (statusQuery.exec()) {
        while (statusQuery.next()) {
            rows.push_back(makeStatisticRow(statusQuery.value(0).toString(),
                                            static_cast<double>(statusQuery.value(1).toInt()),
                                            QStringLiteral("Content items")));
        }
    }

    QSqlQuery publishedQuery{database};
    publishedQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) "
        "FROM publication "
        "WHERE status = 'published'"));
    if (publishedQuery.exec() && publishedQuery.next()) {
        rows.push_back(makeStatisticRow(QStringLiteral("Published items"),
                                        static_cast<double>(publishedQuery.value(0).toInt()),
                                        QStringLiteral("All publication records")));
    }

    const auto today = QDate::currentDate();
    const auto startAt = startOfDayUtc(today.addDays(-6)).toString(Qt::ISODate);
    const auto endAt = endExclusiveUtc(today).toString(Qt::ISODate);

    QSqlQuery newItemsQuery{database};
    newItemsQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) "
        "FROM content "
        "WHERE created_at >= :start_at "
        "AND created_at < :end_at"));
    newItemsQuery.bindValue(":start_at"_L1, startAt);
    newItemsQuery.bindValue(":end_at"_L1, endAt);
    if (newItemsQuery.exec() && newItemsQuery.next()) {
        rows.push_back(makeStatisticRow(QStringLiteral("New items last 7 days"),
                                        static_cast<double>(newItemsQuery.value(0).toInt())));
    }

    QSqlQuery recentPublicationsQuery{database};
    recentPublicationsQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) "
        "FROM publication "
        "WHERE status = 'published' "
        "AND COALESCE(published_at, updated_at) >= :start_at "
        "AND COALESCE(published_at, updated_at) < :end_at"));
    recentPublicationsQuery.bindValue(":start_at"_L1, startAt);
    recentPublicationsQuery.bindValue(":end_at"_L1, endAt);
    if (recentPublicationsQuery.exec() && recentPublicationsQuery.next()) {
        rows.push_back(makeStatisticRow(QStringLiteral("Publications last 7 days"),
                                        static_cast<double>(recentPublicationsQuery.value(0).toInt())));
    }

    return rows;
}

QString pluralizeDays(const int days)
{
    return days == 1 ? QStringLiteral("1 day") : QStringLiteral("%1 days").arg(days);
}

Domain::DashboardRow makeAlertRow(const Domain::Goal &goal,
                                  const QString &summaryText,
                                  const QString &detailText,
                                  const Domain::DashboardHealth health,
                                  const double severityScore)
{
    return {
        .goalId = goal.id,
        .goalName = goal.name,
        .goalType = goal.goalType,
        .scopeType = goal.scopeType,
        .metricType = goal.metricType,
        .displayName = goal.scopeDisplayName.isEmpty() ? goal.name : goal.scopeDisplayName,
        .summaryText = summaryText,
        .detailText = detailText,
        .severityScore = severityScore,
        .health = health,
    };
}

void appendOrUpgradeFocus(std::map<QString, FocusAccumulator> &items, const Domain::DashboardRow &candidate)
{
    const auto key = candidate.displayName + u'|' + candidate.scopeType;
    auto &accumulator = items[key];
    if (!accumulator.initialized || candidate.severityScore > accumulator.row.severityScore) {
        accumulator.row = candidate;
        accumulator.initialized = true;
    }
}

} // namespace

DashboardService::DashboardService(QSqlDatabase database)
    : database_(std::move(database))
    , goalsRepository_(database_)
{
}

Domain::DashboardPeriod DashboardService::resolvePerformancePeriod(const PeriodSelection &selection) const
{
    return ::SmTool::App::resolvePerformancePeriod(selection);
}

Domain::DashboardPeriod DashboardService::resolvePipelinePeriod(const PeriodSelection &selection) const
{
    return ::SmTool::App::resolvePipelinePeriod(selection);
}

std::vector<Domain::Goal> DashboardService::enabledGoals() const
{
    auto goals = goalsRepository_.listGoals();
    goals.erase(std::remove_if(goals.begin(), goals.end(), [](const auto &goal) { return !goal.enabled; }), goals.end());
    return goals;
}

Domain::DashboardEvaluation DashboardService::evaluate(const PeriodSelection &performanceSelection,
                                                       const PeriodSelection &pipelineSelection) const
{
    Domain::DashboardEvaluation evaluation;
    evaluation.performancePeriod = resolvePerformancePeriod(performanceSelection);
    evaluation.pipelinePeriod = resolvePipelinePeriod(pipelineSelection);

    const auto goals = enabledGoals();
    std::map<QString, FocusAccumulator> focusCandidates;

    for (const auto &goal : goals) {
        if (goal.goalType != "balance"_L1) {
            const auto expectedCount = approximatePeriodCount(goal, evaluation.performancePeriod) * goal.targetValue;
            if (expectedCount > 0.0) {
                const auto actualCount = countActualForGoal(database_, goal, evaluation.performancePeriod);
                const auto percent = roundOneDecimal((actualCount / expectedCount) * 100.0);
                auto row = Domain::DashboardRow{
                    .goalId = goal.id,
                    .goalName = goal.name,
                    .goalType = goal.goalType,
                    .scopeType = goal.scopeType,
                    .metricType = goal.metricType,
                    .displayName = goal.scopeDisplayName.isEmpty() ? goal.name : goal.scopeDisplayName,
                    .summaryText = goal.summaryText,
                    .detailText = QStringLiteral("Expected %1, actual %2 in %3")
                                      .arg(roundOneDecimal(expectedCount), 0, 'f', expectedCount == std::floor(expectedCount) ? 0 : 1)
                                      .arg(roundOneDecimal(actualCount), 0, 'f', 1)
                                      .arg(detailRangeLabel(evaluation.performancePeriod)),
                    .targetValue = roundOneDecimal(expectedCount),
                    .actualValue = roundOneDecimal(actualCount),
                    .percent = percent,
                    .shortfallValue = std::max(0.0, roundOneDecimal(expectedCount - actualCount)),
                    .health = classifyGoalHealth(percent),
                };
                evaluation.goalAchievement.push_back(row);

                if (row.health != Domain::DashboardHealth::Good && row.health != Domain::DashboardHealth::TooHigh) {
                    const auto severity = std::max(0.0, 100.0 - row.percent);
                    auto alert = makeAlertRow(goal,
                                              QStringLiteral("%1 below target by %2%")
                                                  .arg(row.displayName)
                                                  .arg(roundOneDecimal(std::max(0.0, 100.0 - row.percent)), 0, 'f', 1),
                                              row.detailText,
                                              row.health,
                                              severity);
                    evaluation.alerts.push_back(alert);
                    appendOrUpgradeFocus(focusCandidates, alert);
                }

                if (goal.goalType == "cadence"_L1) {
                    const auto lastPublished = latestPublishedAt(database_, goal);
                    const auto daysSinceLast = lastPublished.isValid() ? lastPublished.date().daysTo(QDate::currentDate()) : -1;
                    const auto cadenceDays = goal.periodType == "rolling_days"_L1
                        ? std::max(goal.periodValue, 1)
                        : goal.periodType == "week"_L1
                            ? std::max(goal.periodValue, 1) * 7
                            : goal.periodType == "month"_L1
                                ? std::max(goal.periodValue, 1) * 30
                                : goal.periodType == "quarter"_L1
                                    ? std::max(goal.periodValue, 1) * 91
                                    : goal.periodType == "year"_L1 ? std::max(goal.periodValue, 1) * 365
                                                                    : std::max(goal.periodValue, 1);
                    const auto staleDays = daysSinceLast >= 0 ? daysSinceLast : cadenceDays * 2;
                    if (staleDays > cadenceDays) {
                        auto health = staleDays <= static_cast<int>(std::round(cadenceDays * 1.25))
                            ? Domain::DashboardHealth::Warning
                            : staleDays <= static_cast<int>(std::round(cadenceDays * 1.75))
                                ? Domain::DashboardHealth::Bad
                                : Domain::DashboardHealth::Critical;
                        auto alert = makeAlertRow(goal,
                                                  QStringLiteral("%1 cadence missed").arg(row.displayName),
                                                  QStringLiteral("%1 not published in %2")
                                                      .arg(row.displayName)
                                                      .arg(pluralizeDays(staleDays)),
                                                  health,
                                                  static_cast<double>(staleDays));
                        evaluation.alerts.push_back(alert);
                        appendOrUpgradeFocus(focusCandidates, alert);
                    }
                }
            }

            const auto requiredPipeline = approximatePeriodCount(goal, evaluation.pipelinePeriod) * goal.targetValue;
            if (requiredPipeline > 0.0) {
                const auto pipelineCount = countPipelineForGoal(database_, goal, evaluation.pipelinePeriod);
                const auto coverage = roundOneDecimal((pipelineCount / requiredPipeline) * 100.0);
                auto row = Domain::DashboardRow{
                    .goalId = goal.id,
                    .goalName = goal.name,
                    .goalType = goal.goalType,
                    .scopeType = goal.scopeType,
                    .metricType = goal.metricType,
                    .displayName = goal.scopeDisplayName.isEmpty() ? goal.name : goal.scopeDisplayName,
                    .summaryText = goal.summaryText,
                    .detailText = QStringLiteral("Required %1, pipeline %2 for %3")
                                      .arg(roundOneDecimal(requiredPipeline), 0, 'f', requiredPipeline == std::floor(requiredPipeline) ? 0 : 1)
                                      .arg(roundOneDecimal(pipelineCount), 0, 'f', 1)
                                      .arg(detailRangeLabel(evaluation.pipelinePeriod)),
                    .percent = coverage,
                    .requiredValue = roundOneDecimal(requiredPipeline),
                    .pipelineValue = roundOneDecimal(pipelineCount),
                    .shortfallValue = std::max(0.0, roundOneDecimal(requiredPipeline - pipelineCount)),
                    .health = classifyPipelineHealth(coverage),
                };
                evaluation.pipelineCoverage.push_back(row);

                if (row.health != Domain::DashboardHealth::Good && row.health != Domain::DashboardHealth::TooHigh) {
                    const auto missingText = goal.metricType == "publication_count"_L1 ? QStringLiteral("scheduled publications")
                                                                                        : QStringLiteral("pipeline items");
                    auto alert = makeAlertRow(goal,
                                              QStringLiteral("%1 has %2% pipeline coverage")
                                                  .arg(row.displayName)
                                                  .arg(roundOneDecimal(coverage), 0, 'f', 1),
                                              QStringLiteral("%1 needs %2 more %3 for %4")
                                                  .arg(row.displayName)
                                                  .arg(roundOneDecimal(row.shortfallValue), 0, 'f', 1)
                                                  .arg(missingText)
                                                  .arg(detailRangeLabel(evaluation.pipelinePeriod)),
                                              row.health,
                                              std::max(0.0, 100.0 - row.percent));
                    evaluation.alerts.push_back(alert);
                    appendOrUpgradeFocus(focusCandidates, alert);
                }
            }
        }

        if (goal.goalType == "balance"_L1) {
            const auto balanceItems = goalsRepository_.listBalanceItems(goal.id);
            double totalWeight = 0.0;
            for (const auto &item : balanceItems) {
                if (item.weight > 0) {
                    totalWeight += static_cast<double>(item.weight);
                }
            }

            std::vector<std::pair<Domain::GoalBalanceItem, double>> actualCounts;
            double totalActual = 0.0;
            actualCounts.reserve(balanceItems.size());
            for (const auto &item : balanceItems) {
                if (item.weight <= 0) {
                    continue;
                }
                const auto actualCount = goal.scopeType == "channel"_L1
                    ? countPublications(database_, item.scopeId, CountMode::PublishedPerformance, evaluation.performancePeriod)
                    : countContent(database_, goal.scopeType, item.scopeId, CountMode::PublishedPerformance, evaluation.performancePeriod);
                totalActual += actualCount;
                actualCounts.emplace_back(item, actualCount);
            }

            for (const auto &[item, actualCount] : actualCounts) {
                const auto targetPercent = totalWeight > 0.0 ? roundOneDecimal((static_cast<double>(item.weight) / totalWeight) * 100.0) : 0.0;
                const auto actualPercent = totalActual > 0.0 ? roundOneDecimal((actualCount / totalActual) * 100.0) : 0.0;
                const auto deviation = roundOneDecimal(actualPercent - targetPercent);
                auto row = Domain::DashboardRow{
                    .goalId = goal.id,
                    .goalName = goal.name,
                    .goalType = goal.goalType,
                    .scopeType = goal.scopeType,
                    .metricType = goal.metricType,
                    .displayName = item.scopeDisplayName,
                    .summaryText = goal.name,
                    .detailText = QStringLiteral("Target %1%, actual %2% in %3")
                                      .arg(targetPercent, 0, 'f', 1)
                                      .arg(actualPercent, 0, 'f', 1)
                                      .arg(detailRangeLabel(evaluation.performancePeriod)),
                    .targetValue = targetPercent,
                    .actualValue = actualPercent,
                    .deviation = deviation,
                    .absoluteDeviation = std::abs(deviation),
                    .health = classifyBalanceHealth(std::abs(deviation)),
                };
                evaluation.balanceDeviation.push_back(row);

                if (deviation < -10.0) {
                    auto alert = makeAlertRow(goal,
                                              QStringLiteral("%1 below balance target by %2 points")
                                                  .arg(item.scopeDisplayName)
                                                  .arg(std::abs(deviation), 0, 'f', 1),
                                              row.detailText,
                                              row.health,
                                              row.absoluteDeviation);
                    evaluation.alerts.push_back(alert);
                    appendOrUpgradeFocus(focusCandidates, alert);
                }
            }
        }
    }

    std::sort(evaluation.goalAchievement.begin(), evaluation.goalAchievement.end(), [](const auto &left, const auto &right) {
        if (healthRank(left.health) != healthRank(right.health)) {
            return healthRank(left.health) < healthRank(right.health);
        }
        return left.percent < right.percent;
    });
    std::sort(evaluation.pipelineCoverage.begin(), evaluation.pipelineCoverage.end(), [](const auto &left, const auto &right) {
        if (healthRank(left.health) != healthRank(right.health)) {
            return healthRank(left.health) < healthRank(right.health);
        }
        return left.percent < right.percent;
    });
    std::sort(evaluation.balanceDeviation.begin(), evaluation.balanceDeviation.end(), [](const auto &left, const auto &right) {
        if (healthRank(left.health) != healthRank(right.health)) {
            return healthRank(left.health) < healthRank(right.health);
        }
        return left.absoluteDeviation > right.absoluteDeviation;
    });
    std::sort(evaluation.alerts.begin(), evaluation.alerts.end(), [](const auto &left, const auto &right) {
        if (healthRank(left.health) != healthRank(right.health)) {
            return healthRank(left.health) < healthRank(right.health);
        }
        return left.severityScore > right.severityScore;
    });

    for (const auto &[key, accumulator] : focusCandidates) {
        Q_UNUSED(key);
        if (accumulator.initialized) {
            evaluation.recommendedFocus.push_back(accumulator.row);
        }
    }
    std::sort(evaluation.recommendedFocus.begin(), evaluation.recommendedFocus.end(), [](const auto &left, const auto &right) {
        if (healthRank(left.health) != healthRank(right.health)) {
            return healthRank(left.health) < healthRank(right.health);
        }
        return left.severityScore > right.severityScore;
    });
    if (evaluation.recommendedFocus.size() > 5) {
        evaluation.recommendedFocus.resize(5);
    }

    evaluation.statistics = collectStatistics(database_);

    for (auto *rows : {&evaluation.goalAchievement, &evaluation.pipelineCoverage, &evaluation.balanceDeviation, &evaluation.alerts, &evaluation.recommendedFocus, &evaluation.statistics}) {
        for (auto &row : *rows) {
            row.detailText = row.detailText.trimmed();
            row.summaryText = row.summaryText.trimmed();
            row.displayName = row.displayName.trimmed();
        }
    }
    return evaluation;
}

} // namespace SmTool::App
