#include "models/contentlistmodel.h"

namespace SmTool::Models {

ContentListModel::ContentListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContentListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant ContentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case ParentIdRole:
        return item.parentId;
    case BurstTemplateKeyRole:
        return item.burstTemplateKey;
    case TitleRole:
        return item.title;
    case DescriptionRole:
        return item.description;
    case PillarRole:
        return item.pillarName;
    case SeriesRole:
        return item.seriesName;
    case KindRole:
        return item.kindName;
    case OutcomeRole:
        return item.outcomeName;
    case SuggestedChannelRole:
        return item.suggestedChannelName;
    case StatusRole:
        return item.status;
    case PriorityRole:
        return item.priority;
    case ScheduledAtRole:
        return item.scheduledAt;
    case PublishedAtRole:
        return item.publishedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> ContentListModel::roleNames() const
{
    return {
        {IdRole, "itemId"},
        {ParentIdRole, "parentId"},
        {BurstTemplateKeyRole, "burstTemplateKey"},
        {TitleRole, "title"},
        {DescriptionRole, "description"},
        {PillarRole, "pillar"},
        {SeriesRole, "series"},
        {KindRole, "kind"},
        {OutcomeRole, "outcome"},
        {SuggestedChannelRole, "suggestedChannel"},
        {StatusRole, "status"},
        {PriorityRole, "priority"},
        {ScheduledAtRole, "scheduledAt"},
        {PublishedAtRole, "publishedAt"},
    };
}

void ContentListModel::setItems(std::vector<Domain::ContentSummary> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
