#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class CalendarRepository
{
public:
    explicit CalendarRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::CalendarEntry> calendarEntries(bool includeArchived,
                                                                     bool includePublished,
                                                                     const QString &searchQuery = {}) const;

private:
    QSqlDatabase database_;
};

} // namespace SmTool::Data
