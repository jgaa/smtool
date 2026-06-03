#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class LookupsRepository
{
public:
    explicit LookupsRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::LookupValue> activeLookups(const QString &tableName) const;
    [[nodiscard]] QString lookupIdByKey(const QString &tableName, const QString &key) const;

private:
    QSqlDatabase database_;
};

} // namespace SmTool::Data
