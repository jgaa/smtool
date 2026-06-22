#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class LookupsRepository
{
public:
    explicit LookupsRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::LookupValue> allLookups(const QString &tableName) const;
    [[nodiscard]] std::vector<Domain::LookupValue> activeLookups(const QString &tableName) const;
    [[nodiscard]] std::vector<Domain::ContentStatus> contentStatuses() const;
    [[nodiscard]] std::vector<Domain::LookupValue> tags() const;
    [[nodiscard]] std::vector<Domain::LookupValue> series(bool includeArchived = true) const;
    [[nodiscard]] QString lookupIdByKey(const QString &tableName, const QString &key) const;
    [[nodiscard]] Domain::LookupValue lookupById(const QString &tableName, const QString &id) const;
    [[nodiscard]] Domain::ContentStatus contentStatusById(const QString &id) const;
    [[nodiscard]] bool lookupKeyExists(const QString &tableName,
                                       const QString &key,
                                       const QString &excludeId = {}) const;
    [[nodiscard]] bool contentStatusIdExists(const QString &id,
                                             const QString &excludeId = {}) const;
    QString createChannel(const Domain::LookupValue &channel, QString *errorMessage);
    bool updateChannel(const Domain::LookupValue &channel, QString *errorMessage);
    bool reorderChannels(const QStringList &orderedIds, QString *errorMessage);
    bool deleteChannel(const QString &channelId, QString *errorMessage);
    QString createPillar(const Domain::LookupValue &pillar, QString *errorMessage);
    bool updatePillar(const Domain::LookupValue &pillar, QString *errorMessage);
    bool reorderPillars(const QStringList &orderedIds, QString *errorMessage);
    bool deletePillar(const QString &pillarId, QString *errorMessage);
    QString createContentKind(const Domain::LookupValue &contentKind, QString *errorMessage);
    bool updateContentKind(const Domain::LookupValue &contentKind, QString *errorMessage);
    bool reorderContentKinds(const QStringList &orderedIds, QString *errorMessage);
    bool deleteContentKind(const QString &contentKindId, QString *errorMessage);
    QString createOutcome(const Domain::LookupValue &outcome, QString *errorMessage);
    bool updateOutcome(const Domain::LookupValue &outcome, QString *errorMessage);
    bool reorderOutcomes(const QStringList &orderedIds, QString *errorMessage);
    bool deleteOutcome(const QString &outcomeId, QString *errorMessage);
    QString createContentStatus(const Domain::ContentStatus &contentStatus, QString *errorMessage);
    bool updateContentStatus(const QString &existingId,
                             const Domain::ContentStatus &contentStatus,
                             QString *errorMessage);
    bool reorderContentStatuses(const QStringList &orderedIds, QString *errorMessage);
    bool deleteContentStatus(const QString &contentStatusId, QString *errorMessage);

private:
    bool syncChannelFanOutTemplates(QString *errorMessage);
    [[nodiscard]] int lookupReferenceCount(const QString &tableName,
                                           const QString &columnName,
                                           const QString &value,
                                           QString *errorMessage = nullptr) const;

    QSqlDatabase database_;
};

} // namespace SmTool::Data
