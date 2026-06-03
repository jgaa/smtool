#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class CalendarEntryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ContentIdRole,
        TitleRole,
        SeriesRole,
        ChannelRole,
        SourceTypeRole,
        ScheduledAtRole,
    };
    Q_ENUM(Roles)

    explicit CalendarEntryModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::CalendarEntry> items);

private:
    std::vector<Domain::CalendarEntry> items_;
};

} // namespace SmTool::Models
