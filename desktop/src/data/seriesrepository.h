#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class SeriesRepository
{
public:
    explicit SeriesRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::SeriesSummary> list(bool includeArchived, const QString &searchQuery = {}) const;
    [[nodiscard]] Domain::SeriesDetail getById(const QString &id) const;
    [[nodiscard]] QString create(const QString &name,
                                 const QString &description,
                                 const QString &pillarId,
                                 const QString &status,
                                 QString *errorMessage = nullptr) const;
    bool update(const Domain::SeriesDetail &series, QString *errorMessage = nullptr) const;
    bool archive(const QString &id, QString *errorMessage = nullptr) const;
    bool remove(const QString &id, QString *errorMessage = nullptr) const;

private:
    QSqlDatabase database_;
};

} // namespace SmTool::Data
