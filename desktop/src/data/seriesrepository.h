#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class SeriesRepository
{
public:
    explicit SeriesRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::SeriesSummary> list(bool includeArchived) const;
    [[nodiscard]] QString create(const QString &name,
                                 const QString &description,
                                 const QString &pillarId,
                                 const QString &status,
                                 QString *errorMessage = nullptr) const;

private:
    QSqlDatabase database_;
};

} // namespace SmTool::Data
