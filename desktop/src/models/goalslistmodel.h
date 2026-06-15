#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class GoalsListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        GoalTypeRole,
        ScopeTypeRole,
        ScopeIdRole,
        ScopeDisplayNameRole,
        MetricTypeRole,
        TargetValueRole,
        PeriodTypeRole,
        PeriodValueRole,
        EnabledRole,
        SummaryTextRole,
    };
    Q_ENUM(Roles)

    explicit GoalsListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::Goal> items);

private:
    std::vector<Domain::Goal> items_;
};

} // namespace SmTool::Models
