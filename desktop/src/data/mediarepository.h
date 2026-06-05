#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class MediaRepository
{
public:
    explicit MediaRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::MediaItem> listForContent(const QString &contentId) const;
    [[nodiscard]] std::vector<Domain::MediaItem> listForPublication(const QString &publicationId) const;
    bool replaceForContent(const QString &contentId,
                           const std::vector<Domain::MediaItem> &items,
                           QString *errorMessage = nullptr) const;
    bool replaceForPublication(const QString &publicationId,
                               const std::vector<Domain::MediaItem> &items,
                               QString *errorMessage = nullptr) const;

private:
    [[nodiscard]] std::vector<Domain::MediaItem> listForOwner(const QString &ownerColumn, const QString &ownerId) const;
    bool replaceForOwner(const QString &ownerColumn,
                         const QString &ownerId,
                         const std::vector<Domain::MediaItem> &items,
                         QString *errorMessage) const;

    QSqlDatabase database_;
};

} // namespace SmTool::Data
