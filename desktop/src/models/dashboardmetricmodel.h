#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class DashboardMetricModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        LabelRole = Qt::UserRole + 1,
        ValueRole,
        SecondaryRole,
    };
    Q_ENUM(Roles)

    explicit DashboardMetricModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::DashboardMetric> items);

private:
    std::vector<Domain::DashboardMetric> items_;
};

} // namespace SmTool::Models
