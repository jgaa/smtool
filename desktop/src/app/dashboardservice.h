#pragma once

#include "data/goalsrepository.h"
#include "domain/entities.h"

#include <QSqlDatabase>

namespace SmTool::App {

class DashboardService
{
public:
    struct PeriodSelection {
        QString key;
        QString startDate;
        QString endDate;
    };

    explicit DashboardService(QSqlDatabase database);

    [[nodiscard]] Domain::DashboardPeriod resolvePerformancePeriod(const PeriodSelection &selection) const;
    [[nodiscard]] Domain::DashboardPeriod resolvePipelinePeriod(const PeriodSelection &selection) const;
    [[nodiscard]] Domain::DashboardEvaluation evaluate(const PeriodSelection &performanceSelection,
                                                       const PeriodSelection &pipelineSelection) const;

private:
    [[nodiscard]] std::vector<Domain::Goal> enabledGoals() const;

    QSqlDatabase database_;
    Data::GoalsRepository goalsRepository_;
};

} // namespace SmTool::App
