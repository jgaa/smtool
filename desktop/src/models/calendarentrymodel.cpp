#include "models/calendarentrymodel.h"

#include <QVariantMap>

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
    case ContentStatusRole:
        return item.contentStatus;
    case PublicationStatusRole:
        return item.publicationStatus;
    case ScheduledAtRole:
        return item.scheduledAt;
    case IsOverdueRole:
        return item.isOverdue;
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
        {ContentStatusRole, "contentStatus"},
        {PublicationStatusRole, "publicationStatus"},
        {ScheduledAtRole, "scheduledAt"},
        {IsOverdueRole, "isOverdue"},
    };
}

int CalendarEntryModel::count() const
{
    return rowCount();
}

QVariantMap CalendarEntryModel::entryAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    const auto &item = items_.at(row);
    return {
        {QStringLiteral("entryId"), item.id},
        {QStringLiteral("contentId"), item.contentId},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("series"), item.seriesName},
        {QStringLiteral("channel"), item.channelName},
        {QStringLiteral("sourceType"), item.sourceType},
        {QStringLiteral("contentStatus"), item.contentStatus},
        {QStringLiteral("publicationStatus"), item.publicationStatus},
        {QStringLiteral("scheduledAt"), item.scheduledAt},
        {QStringLiteral("isOverdue"), item.isOverdue},
    };
}

void CalendarEntryModel::setItems(std::vector<Domain::CalendarEntry> items)
{
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

} // namespace SmTool::Models
