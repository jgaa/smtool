#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>
#include <QStringList>

#include <vector>

namespace SmTool::Data {

class PublicationRepository
{
public:
    explicit PublicationRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::Publication> listForContent(const QString &contentId) const;
    [[nodiscard]] Domain::Publication getById(const QString &id) const;
    [[nodiscard]] QString create(const Domain::Publication &publication, QString *errorMessage = nullptr) const;
    bool createMissingForContent(const QString &contentId,
                                 const QStringList &channelIds,
                                 int *createdCount = nullptr,
                                 QString *errorMessage = nullptr) const;
    bool update(const Domain::Publication &publication, QString *errorMessage = nullptr) const;
    bool remove(const QString &id, QString *errorMessage = nullptr) const;

private:
    QSqlDatabase database_;
};

} // namespace SmTool::Data
