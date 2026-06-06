#pragma once

#include "domain/entities.h"

#include <QAbstractListModel>

#include <vector>

namespace SmTool::Models {

class DashboardRowModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        GoalIdRole = Qt::UserRole + 1,
        GoalNameRole,
        GoalTypeRole,
        ScopeTypeRole,
        MetricTypeRole,
        DisplayNameRole,
        SummaryTextRole,
        DetailTextRole,
        TargetValueRole,
        ActualValueRole,
        PercentRole,
        DeviationRole,
        AbsoluteDeviationRole,
        RequiredValueRole,
        PipelineValueRole,
        ShortfallValueRole,
        SeverityScoreRole,
        HealthRole,
        HealthColorRole,
    };
    Q_ENUM(Roles)

    explicit DashboardRowModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Domain::DashboardRow> items);

private:
    [[nodiscard]] static QString healthKey(Domain::DashboardHealth health);
    [[nodiscard]] static QString healthColor(Domain::DashboardHealth health);

    std::vector<Domain::DashboardRow> items_;
};

} // namespace SmTool::Models
