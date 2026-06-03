#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class ContentRepository
{
public:
    explicit ContentRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::ContentSummary> inboxItems() const;
    [[nodiscard]] std::vector<Domain::ContentSummary> boardItems(const QString &status, bool includeArchived) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> rootItems(bool includeArchived) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> childItems(const QString &parentId) const;

    [[nodiscard]] QString create(const Domain::ContentItem &content, QString *errorMessage = nullptr) const;
    bool updateStatus(const QString &id, const QString &newStatus, QString *errorMessage = nullptr) const;
    bool createBurst(const QString &sourceContentId, QString *errorMessage = nullptr) const;

private:
    [[nodiscard]] std::vector<Domain::ContentSummary> runSummaryQuery(QSqlQuery &query) const;
    [[nodiscard]] Domain::ContentItem getContentById(const QString &id) const;

    QSqlDatabase database_;
};

} // namespace SmTool::Data
