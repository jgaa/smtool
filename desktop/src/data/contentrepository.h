#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class ContentRepository
{
public:
    enum class SortMode {
        DueDateAlphabetical,
        PriorityAlphabetical,
        Alphabetical,
        StatusAlphabetical,
        StatusDueDate,
        StatusFirstPublishDate,
        PillarAlphabetical,
        PillarDueDate,
        PillarFirstPublishDate,
    };

    explicit ContentRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::ContentSummary> inboxItems(const QString &searchQuery = {}) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> boardItems(const QString &status, bool includeArchived, const QString &searchQuery = {}) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> allItems(bool includeArchived, SortMode sortMode, const QString &searchQuery = {}) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> rootItems(bool includeArchived, const QString &searchQuery = {}) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> childItems(const QString &parentId, const QString &searchQuery = {}) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> listContentForSeries(const QString &seriesId, const QString &searchQuery = {}) const;
    [[nodiscard]] std::vector<Domain::BurstTemplate> activeBurstTemplates() const;

    [[nodiscard]] QString create(const Domain::ContentItem &content, QString *errorMessage = nullptr) const;
    [[nodiscard]] QString createInSeries(const QString &seriesId, Domain::ContentItem content, QString *errorMessage = nullptr) const;
    bool update(const Domain::ContentItem &content, QString *errorMessage = nullptr) const;
    bool updateStatus(const QString &id, const QString &newStatus, QString *errorMessage = nullptr) const;
    bool assignContentToSeries(const QString &contentId, const QString &seriesId, QString *errorMessage = nullptr) const;
    bool removeContentFromSeries(const QString &contentId, QString *errorMessage = nullptr) const;
    bool setSeriesPosition(const QString &contentId, int position, QString *errorMessage = nullptr) const;
    bool moveSeriesItem(const QString &seriesId, const QString &contentId, int direction, QString *errorMessage = nullptr) const;
    bool remove(const QString &id, QString *errorMessage = nullptr) const;
    bool createBurst(const QString &sourceContentId, QString *errorMessage = nullptr) const;
    bool createBurst(const QString &sourceContentId, const QStringList &templateKeys, QString *errorMessage = nullptr) const;
    [[nodiscard]] Domain::ContentItem getById(const QString &id) const;

private:
    [[nodiscard]] std::vector<Domain::ContentSummary> runSummaryQuery(QSqlQuery &query) const;
    [[nodiscard]] Domain::ContentItem getContentById(const QString &id) const;
    [[nodiscard]] QStringList contentTags(const QString &contentId) const;
    [[nodiscard]] std::vector<Domain::ContentSummary> filteredItems(std::vector<Domain::ContentSummary> items,
                                                                    const QString &searchQuery) const;
    [[nodiscard]] int nextSeriesPosition(const QString &seriesId) const;
    [[nodiscard]] bool seriesExists(const QString &seriesId) const;
    [[nodiscard]] bool seriesIsArchived(const QString &seriesId) const;
    bool normalizeSeriesPositions(const QString &seriesId, QString *errorMessage = nullptr) const;

    QSqlDatabase database_;
};

} // namespace SmTool::Data
