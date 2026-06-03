#include "models/dashboardmetricmodel.h"

namespace SmTool::Models {

DashboardMetricModel::DashboardMetricModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DashboardMetricModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant DashboardMetricModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case LabelRole:
        return item.label;
    case ValueRole:
        return item.value;
    case SecondaryRole:
        return item.secondary;
    default:
        return {};
    }
}

QHash<int, QByteArray> DashboardMetricModel::roleNames() const
{
    return {
        {LabelRole, "label"},
        {ValueRole, "value"},
        {SecondaryRole, "secondary"},
    };
}

void DashboardMetricModel::setItems(std::vector<Domain::DashboardMetric> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
