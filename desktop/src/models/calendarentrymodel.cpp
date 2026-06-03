#include "models/calendarentrymodel.h"

namespace SmTool::Models {

CalendarEntryModel::CalendarEntryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CalendarEntryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant CalendarEntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case ContentIdRole:
        return item.contentId;
    case TitleRole:
        return item.title;
    case SeriesRole:
        return item.seriesName;
    case ChannelRole:
        return item.channelName;
    case SourceTypeRole:
        return item.sourceType;
    case ScheduledAtRole:
        return item.scheduledAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> CalendarEntryModel::roleNames() const
{
    return {
        {IdRole, "entryId"},
        {ContentIdRole, "contentId"},
        {TitleRole, "title"},
        {SeriesRole, "series"},
        {ChannelRole, "channel"},
        {SourceTypeRole, "sourceType"},
        {ScheduledAtRole, "scheduledAt"},
    };
}

void CalendarEntryModel::setItems(std::vector<Domain::CalendarEntry> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
