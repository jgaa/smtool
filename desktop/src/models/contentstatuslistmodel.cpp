#include "models/contentstatuslistmodel.h"

#include "domain/constants.h"

namespace SmTool::Models {

ContentStatusListModel::ContentStatusListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContentStatusListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant ContentStatusListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case DisplayNameRole:
        return Domain::titleFromKey(item.id);
    case InfoRole:
        return item.info;
    case SortOrderRole:
        return item.sortOrder;
    case SystemRole:
        return item.isSystem;
    default:
        return {};
    }
}

QHash<int, QByteArray> ContentStatusListModel::roleNames() const
{
    return {
        {IdRole, "statusId"},
        {DisplayNameRole, "displayName"},
        {InfoRole, "info"},
        {SortOrderRole, "sortOrder"},
        {SystemRole, "isSystem"},
    };
}

void ContentStatusListModel::setItems(std::vector<Domain::ContentStatus> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

const std::vector<Domain::ContentStatus> &ContentStatusListModel::items() const
{
    return items_;
}

} // namespace SmTool::Models
