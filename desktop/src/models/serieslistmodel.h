#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class SeriesListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        PillarRole,
        StatusRole,
        ContentCountRole,
        ScheduledCountRole,
    };
    Q_ENUM(Roles)

    explicit SeriesListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::SeriesSummary> items);

private:
    std::vector<Domain::SeriesSummary> items_;
};

} // namespace SmTool::Models
