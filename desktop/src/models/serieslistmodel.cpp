#include "models/serieslistmodel.h"

namespace SmTool::Models {

SeriesListModel::SeriesListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SeriesListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant SeriesListModel::data(const QModelIndex &index, int role) const
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
    case DescriptionRole:
        return item.description;
    case PillarRole:
        return item.pillarName;
    case StatusRole:
        return item.status;
    case ContentCountRole:
        return item.contentCount;
    case ScheduledCountRole:
        return item.scheduledCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> SeriesListModel::roleNames() const
{
    return {
        {IdRole, "seriesId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {PillarRole, "pillar"},
        {StatusRole, "status"},
        {ContentCountRole, "contentCount"},
        {ScheduledCountRole, "scheduledCount"},
    };
}

void SeriesListModel::setItems(std::vector<Domain::SeriesSummary> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
