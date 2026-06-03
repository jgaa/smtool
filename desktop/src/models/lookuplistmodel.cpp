#include "models/lookuplistmodel.h"

namespace SmTool::Models {

LookupListModel::LookupListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LookupListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant LookupListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case KeyRole:
        return item.key;
    case DisplayNameRole:
        return item.displayName;
    case DescriptionRole:
        return item.description;
    case SortOrderRole:
        return item.sortOrder;
    case ActiveRole:
        return item.isActive;
    default:
        return {};
    }
}

QHash<int, QByteArray> LookupListModel::roleNames() const
{
    return {
        {IdRole, "lookupId"},
        {KeyRole, "key"},
        {DisplayNameRole, "displayName"},
        {DescriptionRole, "description"},
        {SortOrderRole, "sortOrder"},
        {ActiveRole, "isActive"},
    };
}

void LookupListModel::setItems(std::vector<Domain::LookupValue> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
