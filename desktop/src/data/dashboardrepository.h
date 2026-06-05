#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class DashboardRepository
{
public:
    explicit DashboardRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::CalendarEntry> calendarEntries(bool includeArchived, bool includePublished, const QString &searchQuery = {}) const;
    [[nodiscard]] Domain::DashboardData dashboardData(bool includeArchived) const;

private:
    [[nodiscard]] std::vector<Domain::DashboardMetric> runMetricQuery(QSqlQuery &query) const;

    QSqlDatabase database_;
};

} // namespace SmTool::Data
