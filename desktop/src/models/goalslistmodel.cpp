#include "models/goalslistmodel.h"

namespace SmTool::Models {

GoalsListModel::GoalsListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GoalsListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant GoalsListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case GoalTypeRole:
        return item.goalType;
    case ScopeTypeRole:
        return item.scopeType;
    case ScopeIdRole:
        return item.scopeId;
    case ScopeDisplayNameRole:
        return item.scopeDisplayName;
    case MetricTypeRole:
        return item.metricType;
    case TargetValueRole:
        return item.targetValue;
    case PeriodTypeRole:
        return item.periodType;
    case PeriodValueRole:
        return item.periodValue;
    case EnabledRole:
        return item.enabled;
    case SummaryTextRole:
        return item.summaryText;
    default:
        return {};
    }
}

QHash<int, QByteArray> GoalsListModel::roleNames() const
{
    return {
        {IdRole, "goalId"},
        {NameRole, "name"},
        {GoalTypeRole, "goalType"},
        {ScopeTypeRole, "scopeType"},
        {ScopeIdRole, "scopeId"},
        {ScopeDisplayNameRole, "scopeDisplayName"},
        {MetricTypeRole, "metricType"},
        {TargetValueRole, "targetValue"},
        {PeriodTypeRole, "periodType"},
        {PeriodValueRole, "periodValue"},
        {EnabledRole, "enabled"},
        {SummaryTextRole, "summaryText"},
    };
}

void GoalsListModel::setItems(std::vector<Domain::Goal> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
