#include "models/dashboardrowmodel.h"

namespace SmTool::Models {

DashboardRowModel::DashboardRowModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DashboardRowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant DashboardRowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case GoalIdRole:
        return item.goalId;
    case GoalNameRole:
        return item.goalName;
    case GoalTypeRole:
        return item.goalType;
    case ScopeTypeRole:
        return item.scopeType;
    case MetricTypeRole:
        return item.metricType;
    case DisplayNameRole:
        return item.displayName;
    case SummaryTextRole:
        return item.summaryText;
    case DetailTextRole:
        return item.detailText;
    case TargetValueRole:
        return item.targetValue;
    case ActualValueRole:
        return item.actualValue;
    case PercentRole:
        return item.percent;
    case DeviationRole:
        return item.deviation;
    case AbsoluteDeviationRole:
        return item.absoluteDeviation;
    case RequiredValueRole:
        return item.requiredValue;
    case PipelineValueRole:
        return item.pipelineValue;
    case ShortfallValueRole:
        return item.shortfallValue;
    case SeverityScoreRole:
        return item.severityScore;
    case HealthRole:
        return healthKey(item.health);
    case HealthColorRole:
        return healthColor(item.health);
    default:
        return {};
    }
}

QHash<int, QByteArray> DashboardRowModel::roleNames() const
{
    return {
        {GoalIdRole, "goalId"},
        {GoalNameRole, "goalName"},
        {GoalTypeRole, "goalType"},
        {ScopeTypeRole, "scopeType"},
        {MetricTypeRole, "metricType"},
        {DisplayNameRole, "displayName"},
        {SummaryTextRole, "summaryText"},
        {DetailTextRole, "detailText"},
        {TargetValueRole, "targetValue"},
        {ActualValueRole, "actualValue"},
        {PercentRole, "percent"},
        {DeviationRole, "deviation"},
        {AbsoluteDeviationRole, "absoluteDeviation"},
        {RequiredValueRole, "requiredValue"},
        {PipelineValueRole, "pipelineValue"},
        {ShortfallValueRole, "shortfallValue"},
        {SeverityScoreRole, "severityScore"},
        {HealthRole, "health"},
        {HealthColorRole, "healthColor"},
    };
}

void DashboardRowModel::setItems(std::vector<Domain::DashboardRow> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

QString DashboardRowModel::healthKey(const Domain::DashboardHealth health)
{
    switch (health) {
    case Domain::DashboardHealth::Good:
        return QStringLiteral("good");
    case Domain::DashboardHealth::SlightlyLow:
        return QStringLiteral("slightly_low");
    case Domain::DashboardHealth::Warning:
        return QStringLiteral("warning");
    case Domain::DashboardHealth::Bad:
        return QStringLiteral("bad");
    case Domain::DashboardHealth::Critical:
        return QStringLiteral("critical");
    case Domain::DashboardHealth::TooHigh:
        return QStringLiteral("too_high");
    case Domain::DashboardHealth::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

QString DashboardRowModel::healthColor(const Domain::DashboardHealth health)
{
    switch (health) {
    case Domain::DashboardHealth::Good:
        return QStringLiteral("#2f6f44");
    case Domain::DashboardHealth::SlightlyLow:
        return QStringLiteral("#6f8d2a");
    case Domain::DashboardHealth::Warning:
        return QStringLiteral("#c98a10");
    case Domain::DashboardHealth::Bad:
        return QStringLiteral("#c75b12");
    case Domain::DashboardHealth::Critical:
        return QStringLiteral("#b6382f");
    case Domain::DashboardHealth::TooHigh:
        return QStringLiteral("#2d6ea8");
    case Domain::DashboardHealth::Unknown:
    default:
        return QStringLiteral("#7a7a7a");
    }
}

} // namespace SmTool::Models
