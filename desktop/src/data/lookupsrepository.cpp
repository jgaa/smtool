#include "data/lookupsrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QUuid>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

bool execWithError(QSqlQuery &query, QString *errorMessage)
{
    if (query.exec()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

QString managedFanOutTemplateKey(const QString &channelKey)
{
    return QStringLiteral("fanout_%1").arg(channelKey);
}

QString defaultFanOutKindKey(const QString &channelKey)
{
    if (channelKey == "blog"_L1) {
        return QStringLiteral("blog_post");
    }
    if (channelKey == "youtube"_L1 || channelKey == "tiktok"_L1) {
        return QStringLiteral("clip");
    }
    if (channelKey == "newsletter"_L1) {
        return QStringLiteral("newsletter");
    }
    return QStringLiteral("short_post");
}

QString defaultFanOutOutcomeKey(const QString &channelKey)
{
    if (channelKey == "blog"_L1 || channelKey == "youtube"_L1 || channelKey == "newsletter"_L1 || channelKey == "tiktok"_L1) {
        return QStringLiteral("trust");
    }
    return QStringLiteral("authority");
}

bool isRequiredManagedContentKindKey(const QString &key)
{
    static const QSet<QString> requiredKeys{
        QStringLiteral("short_post"),
        QStringLiteral("clip"),
        QStringLiteral("newsletter"),
        QStringLiteral("blog_post"),
    };
    return requiredKeys.contains(key);
}

bool isRequiredManagedOutcomeKey(const QString &key)
{
    static const QSet<QString> requiredKeys{
        QStringLiteral("authority"),
        QStringLiteral("trust"),
    };
    return requiredKeys.contains(key);
}

QString lookupIdByKeyInternal(QSqlDatabase db, const QString &tableName, const QString &key, QString *errorMessage)
{
    QSqlQuery query{db};
    query.prepare(QStringLiteral("SELECT id FROM %1 WHERE key = :key").arg(tableName));
    query.bindValue(":key"_L1, key);
    if (!execWithError(query, errorMessage)) {
        return {};
    }
    return query.next() ? query.value(0).toString() : QString{};
}

} // namespace

LookupsRepository::LookupsRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::LookupValue> LookupsRepository::allLookups(const QString &tableName) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, key, display_name, COALESCE(description, ''), sort_order, is_active "
        "FROM %1 "
        "ORDER BY sort_order ASC, display_name COLLATE NOCASE ASC, id ASC")
                      .arg(tableName));
    query.exec();

    std::vector<Domain::LookupValue> results;
    while (query.next()) {
        results.push_back({
            .id = query.value(0).toString(),
            .key = query.value(1).toString(),
            .displayName = query.value(2).toString(),
            .description = query.value(3).toString(),
            .sortOrder = query.value(4).toInt(),
            .isActive = query.value(5).toBool(),
        });
    }
    return results;
}

std::vector<Domain::LookupValue> LookupsRepository::activeLookups(const QString &tableName) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, key, display_name, COALESCE(description, ''), sort_order, is_active "
        "FROM %1 "
        "ORDER BY is_active DESC, sort_order ASC, display_name ASC")
                      .arg(tableName));
    query.exec();

    std::vector<Domain::LookupValue> results;
    while (query.next()) {
        results.push_back({
            .id = query.value(0).toString(),
            .key = query.value(1).toString(),
            .displayName = query.value(2).toString(),
            .description = query.value(3).toString(),
            .sortOrder = query.value(4).toInt(),
            .isActive = query.value(5).toBool(),
        });
    }
    return results;
}

std::vector<Domain::ContentStatus> LookupsRepository::contentStatuses() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, COALESCE(info, ''), sort_order, is_system "
        "FROM content_status "
        "ORDER BY sort_order ASC, id ASC"));
    query.exec();

    std::vector<Domain::ContentStatus> results;
    while (query.next()) {
        results.push_back({
            .id = query.value(0).toString(),
            .info = query.value(1).toString(),
            .sortOrder = query.value(2).toInt(),
            .isSystem = query.value(3).toBool(),
        });
    }
    return results;
}

std::vector<Domain::LookupValue> LookupsRepository::tags() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id "
        "FROM tag "
        "ORDER BY id ASC"));
    query.exec();

    std::vector<Domain::LookupValue> results;
    while (query.next()) {
        const auto id = query.value(0).toString();
        results.push_back({
            .id = id,
            .key = id,
            .displayName = QStringLiteral("#%1").arg(id),
            .description = {},
            .sortOrder = static_cast<int>(results.size()),
            .isActive = true,
        });
    }
    return results;
}

std::vector<Domain::LookupValue> LookupsRepository::series(bool includeArchived) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, name, COALESCE(description, '') "
        "FROM series "
        "WHERE (:include_archived = 1 OR status != 'archived') "
        "ORDER BY name COLLATE NOCASE ASC"));
    query.bindValue(":include_archived", includeArchived ? 1 : 0);
    query.exec();

    std::vector<Domain::LookupValue> results;
    while (query.next()) {
        const auto id = query.value(0).toString();
        results.push_back({
            .id = id,
            .key = id,
            .displayName = query.value(1).toString(),
            .description = query.value(2).toString(),
            .sortOrder = static_cast<int>(results.size()),
            .isActive = true,
        });
    }
    return results;
}

QString LookupsRepository::lookupIdByKey(const QString &tableName, const QString &key) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("SELECT id FROM %1 WHERE key = :key").arg(tableName));
    query.bindValue(QStringLiteral(":key"), key);
    query.exec();
    return query.next() ? query.value(0).toString() : QString{};
}

Domain::LookupValue LookupsRepository::lookupById(const QString &tableName, const QString &id) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, key, display_name, COALESCE(description, ''), sort_order, is_active "
        "FROM %1 WHERE id = :id")
                      .arg(tableName));
    query.bindValue(":id"_L1, id);
    query.exec();
    if (!query.next()) {
        return {};
    }

    return {
        .id = query.value(0).toString(),
        .key = query.value(1).toString(),
        .displayName = query.value(2).toString(),
        .description = query.value(3).toString(),
        .sortOrder = query.value(4).toInt(),
        .isActive = query.value(5).toBool(),
    };
}

Domain::ContentStatus LookupsRepository::contentStatusById(const QString &id) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, COALESCE(info, ''), sort_order, is_system "
        "FROM content_status "
        "WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    query.exec();
    if (!query.next()) {
        return {};
    }

    return {
        .id = query.value(0).toString(),
        .info = query.value(1).toString(),
        .sortOrder = query.value(2).toInt(),
        .isSystem = query.value(3).toBool(),
    };
}

bool LookupsRepository::lookupKeyExists(const QString &tableName,
                                        const QString &key,
                                        const QString &excludeId) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) "
        "FROM %1 "
        "WHERE key = :key AND (:exclude_id = '' OR id != :exclude_id)")
                      .arg(tableName));
    query.bindValue(":key"_L1, key);
    query.bindValue(":exclude_id"_L1, excludeId);
    query.exec();
    return query.next() && query.value(0).toInt() > 0;
}

bool LookupsRepository::contentStatusIdExists(const QString &id, const QString &excludeId) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) "
        "FROM content_status "
        "WHERE id = :id AND (:exclude_id = '' OR id != :exclude_id)"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":exclude_id"_L1, excludeId);
    query.exec();
    return query.next() && query.value(0).toInt() > 0;
}

QString LookupsRepository::createChannel(const Domain::LookupValue &channel, QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return {};
    }

    QSqlQuery shiftQuery{database_};
    shiftQuery.prepare(QStringLiteral("UPDATE channel SET sort_order = sort_order + 1"));
    if (!execWithError(shiftQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery insertQuery{database_};
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO channel (id, key, display_name, description, sort_order, is_active) "
        "VALUES (:id, :key, :display_name, :description, 0, :is_active)"));
    insertQuery.bindValue(":id"_L1, id);
    insertQuery.bindValue(":key"_L1, channel.key);
    insertQuery.bindValue(":display_name"_L1, channel.displayName);
    insertQuery.bindValue(":description"_L1, channel.description);
    insertQuery.bindValue(":is_active"_L1, channel.isActive ? 1 : 0);
    if (!execWithError(insertQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    if (!syncChannelFanOutTemplates(errorMessage)) {
        database_.rollback();
        return {};
    }

    if (database_.commit()) {
        return id;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return {};
}

bool LookupsRepository::updateChannel(const Domain::LookupValue &channel, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("channel"), channel.id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Channel not found.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    const auto oldManagedTemplateKey = managedFanOutTemplateKey(existing.key);
    const auto newManagedTemplateKey = managedFanOutTemplateKey(channel.key);
    if (existing.key != channel.key) {
        const auto referenceCount = lookupReferenceCount(QStringLiteral("content"),
                                                         QStringLiteral("burst_template_key"),
                                                         oldManagedTemplateKey,
                                                         errorMessage);
        if (referenceCount < 0) {
            database_.rollback();
            return false;
        }
        if (referenceCount > 0) {
            database_.rollback();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral(
                    "Cannot change channel key because burst-generated content still uses template \"%1\". "
                    "Update or remove those content items first.")
                                    .arg(oldManagedTemplateKey);
            }
            return false;
        }

        QSqlQuery deleteOldTemplateQuery{database_};
        deleteOldTemplateQuery.prepare(QStringLiteral("DELETE FROM burst_template WHERE key = :key"));
        deleteOldTemplateQuery.bindValue(":key"_L1, oldManagedTemplateKey);
        if (!execWithError(deleteOldTemplateQuery, errorMessage)) {
            database_.rollback();
            return false;
        }

        if (oldManagedTemplateKey != newManagedTemplateKey) {
            QSqlQuery deleteConflictingTemplateQuery{database_};
            deleteConflictingTemplateQuery.prepare(QStringLiteral(
                "DELETE FROM burst_template "
                "WHERE key = :key "
                "AND NOT EXISTS (SELECT 1 FROM content WHERE burst_template_key = :key)"));
            deleteConflictingTemplateQuery.bindValue(":key"_L1, newManagedTemplateKey);
            if (!execWithError(deleteConflictingTemplateQuery, errorMessage)) {
                database_.rollback();
                return false;
            }
        }
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral(
        "UPDATE channel "
        "SET key = :key, display_name = :display_name, is_active = :is_active "
        "WHERE id = :id"));
    updateQuery.bindValue(":key"_L1, channel.key);
    updateQuery.bindValue(":display_name"_L1, channel.displayName);
    updateQuery.bindValue(":is_active"_L1, channel.isActive ? 1 : 0);
    updateQuery.bindValue(":id"_L1, channel.id);
    if (!execWithError(updateQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (!syncChannelFanOutTemplates(errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::deleteChannel(const QString &channelId, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("channel"), channelId.trimmed());
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Channel not found.");
        }
        return false;
    }

    const auto managedTemplateKey = managedFanOutTemplateKey(existing.key);
    const auto publicationCount = lookupReferenceCount(QStringLiteral("publication"),
                                                       QStringLiteral("channel_id"),
                                                       existing.id,
                                                       errorMessage);
    const auto contentSuggestionCount = lookupReferenceCount(QStringLiteral("content"),
                                                             QStringLiteral("suggested_channel_id"),
                                                             existing.id,
                                                             errorMessage);
    const auto burstTemplateUsageCount = lookupReferenceCount(QStringLiteral("content"),
                                                              QStringLiteral("burst_template_key"),
                                                              managedTemplateKey,
                                                              errorMessage);
    if (publicationCount < 0 || contentSuggestionCount < 0 || burstTemplateUsageCount < 0) {
        return false;
    }

    if (publicationCount > 0 || contentSuggestionCount > 0 || burstTemplateUsageCount > 0) {
        if (errorMessage != nullptr) {
            QStringList reasons;
            if (publicationCount > 0) {
                reasons.append(QStringLiteral("%1 publication records").arg(publicationCount));
            }
            if (contentSuggestionCount > 0) {
                reasons.append(QStringLiteral("%1 content suggestions").arg(contentSuggestionCount));
            }
            if (burstTemplateUsageCount > 0) {
                reasons.append(QStringLiteral("%1 burst-generated content items").arg(burstTemplateUsageCount));
            }
            *errorMessage = QStringLiteral("Cannot delete channel because it is still used by %1.")
                                .arg(reasons.join(QStringLiteral(", ")));
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery deleteTemplateQuery{database_};
    deleteTemplateQuery.prepare(QStringLiteral("DELETE FROM burst_template WHERE key = :key"));
    deleteTemplateQuery.bindValue(":key"_L1, managedTemplateKey);
    if (!execWithError(deleteTemplateQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery deleteChannelQuery{database_};
    deleteChannelQuery.prepare(QStringLiteral("DELETE FROM channel WHERE id = :id"));
    deleteChannelQuery.bindValue(":id"_L1, existing.id);
    if (!execWithError(deleteChannelQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery normalizeOrderQuery{database_};
    normalizeOrderQuery.prepare(QStringLiteral(
        "UPDATE channel "
        "SET sort_order = sort_order - 1 "
        "WHERE sort_order > :sort_order"));
    normalizeOrderQuery.bindValue(":sort_order"_L1, existing.sortOrder);
    if (!execWithError(normalizeOrderQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (!syncChannelFanOutTemplates(errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::reorderChannels(const QStringList &orderedIds, QString *errorMessage)
{
    const auto existingChannels = allLookups(QStringLiteral("channel"));
    if (orderedIds.size() != static_cast<qsizetype>(existingChannels.size())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Channel order is incomplete.");
        }
        return false;
    }

    QSet<QString> expectedIds;
    for (const auto &channel : existingChannels) {
        expectedIds.insert(channel.id);
    }
    QSet<QString> actualIds;
    for (const auto &id : orderedIds) {
        actualIds.insert(id);
    }
    if (actualIds != expectedIds) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Channel order must include each channel exactly once.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral("UPDATE channel SET sort_order = :sort_order WHERE id = :id"));
    for (qsizetype index = 0; index < orderedIds.size(); ++index) {
        updateQuery.bindValue(":sort_order"_L1, static_cast<int>(index));
        updateQuery.bindValue(":id"_L1, orderedIds.at(index));
        if (!execWithError(updateQuery, errorMessage)) {
            database_.rollback();
            return false;
        }
    }

    if (!syncChannelFanOutTemplates(errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

QString LookupsRepository::createPillar(const Domain::LookupValue &pillar, QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return {};
    }

    QSqlQuery shiftQuery{database_};
    shiftQuery.prepare(QStringLiteral("UPDATE pillar SET sort_order = sort_order + 1"));
    if (!execWithError(shiftQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery insertQuery{database_};
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO pillar (id, key, display_name, description, sort_order, is_active) "
        "VALUES (:id, :key, :display_name, :description, 0, :is_active)"));
    insertQuery.bindValue(":id"_L1, id);
    insertQuery.bindValue(":key"_L1, pillar.key);
    insertQuery.bindValue(":display_name"_L1, pillar.displayName);
    insertQuery.bindValue(":description"_L1, pillar.description);
    insertQuery.bindValue(":is_active"_L1, pillar.isActive ? 1 : 0);
    if (!execWithError(insertQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    if (database_.commit()) {
        return id;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return {};
}

bool LookupsRepository::updatePillar(const Domain::LookupValue &pillar, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("pillar"), pillar.id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Pillar not found.");
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral(
        "UPDATE pillar "
        "SET key = :key, display_name = :display_name, is_active = :is_active "
        "WHERE id = :id"));
    updateQuery.bindValue(":key"_L1, pillar.key);
    updateQuery.bindValue(":display_name"_L1, pillar.displayName);
    updateQuery.bindValue(":is_active"_L1, pillar.isActive ? 1 : 0);
    updateQuery.bindValue(":id"_L1, pillar.id);
    return execWithError(updateQuery, errorMessage);
}

bool LookupsRepository::deletePillar(const QString &pillarId, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("pillar"), pillarId.trimmed());
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Pillar not found.");
        }
        return false;
    }

    const auto contentCount = lookupReferenceCount(QStringLiteral("content"),
                                                   QStringLiteral("pillar_id"),
                                                   existing.id,
                                                   errorMessage);
    const auto seriesCount = lookupReferenceCount(QStringLiteral("series"),
                                                  QStringLiteral("pillar_id"),
                                                  existing.id,
                                                  errorMessage);
    if (contentCount < 0 || seriesCount < 0) {
        return false;
    }

    if (contentCount > 0 || seriesCount > 0) {
        if (errorMessage != nullptr) {
            QStringList reasons;
            if (contentCount > 0) {
                reasons.append(QStringLiteral("%1 content items").arg(contentCount));
            }
            if (seriesCount > 0) {
                reasons.append(QStringLiteral("%1 series").arg(seriesCount));
            }
            *errorMessage = QStringLiteral("Cannot delete pillar because it is still used by %1.")
                                .arg(reasons.join(QStringLiteral(", ")));
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery deletePillarQuery{database_};
    deletePillarQuery.prepare(QStringLiteral("DELETE FROM pillar WHERE id = :id"));
    deletePillarQuery.bindValue(":id"_L1, existing.id);
    if (!execWithError(deletePillarQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery normalizeOrderQuery{database_};
    normalizeOrderQuery.prepare(QStringLiteral(
        "UPDATE pillar "
        "SET sort_order = sort_order - 1 "
        "WHERE sort_order > :sort_order"));
    normalizeOrderQuery.bindValue(":sort_order"_L1, existing.sortOrder);
    if (!execWithError(normalizeOrderQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::reorderPillars(const QStringList &orderedIds, QString *errorMessage)
{
    const auto existingPillars = allLookups(QStringLiteral("pillar"));
    if (orderedIds.size() != static_cast<qsizetype>(existingPillars.size())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Pillar order is incomplete.");
        }
        return false;
    }

    QSet<QString> expectedIds;
    for (const auto &pillar : existingPillars) {
        expectedIds.insert(pillar.id);
    }
    QSet<QString> actualIds;
    for (const auto &id : orderedIds) {
        actualIds.insert(id);
    }
    if (actualIds != expectedIds) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Pillar order must include each pillar exactly once.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral("UPDATE pillar SET sort_order = :sort_order WHERE id = :id"));
    for (qsizetype index = 0; index < orderedIds.size(); ++index) {
        updateQuery.bindValue(":sort_order"_L1, static_cast<int>(index));
        updateQuery.bindValue(":id"_L1, orderedIds.at(index));
        if (!execWithError(updateQuery, errorMessage)) {
            database_.rollback();
            return false;
        }
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

QString LookupsRepository::createContentKind(const Domain::LookupValue &contentKind, QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return {};
    }

    QSqlQuery shiftQuery{database_};
    shiftQuery.prepare(QStringLiteral("UPDATE content_kind SET sort_order = sort_order + 1"));
    if (!execWithError(shiftQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery insertQuery{database_};
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO content_kind (id, key, display_name, description, sort_order, is_active) "
        "VALUES (:id, :key, :display_name, :description, 0, :is_active)"));
    insertQuery.bindValue(":id"_L1, id);
    insertQuery.bindValue(":key"_L1, contentKind.key);
    insertQuery.bindValue(":display_name"_L1, contentKind.displayName);
    insertQuery.bindValue(":description"_L1, contentKind.description);
    insertQuery.bindValue(":is_active"_L1, contentKind.isActive ? 1 : 0);
    if (!execWithError(insertQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    if (database_.commit()) {
        return id;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return {};
}

bool LookupsRepository::updateContentKind(const Domain::LookupValue &contentKind, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("content_kind"), contentKind.id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content kind not found.");
        }
        return false;
    }

    if (existing.key != contentKind.key && isRequiredManagedContentKindKey(existing.key)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Cannot change this content kind key because the app depends on \"%1\" for built-in fan-out templates.")
                                .arg(existing.key);
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral(
        "UPDATE content_kind "
        "SET key = :key, display_name = :display_name, is_active = :is_active "
        "WHERE id = :id"));
    updateQuery.bindValue(":key"_L1, contentKind.key);
    updateQuery.bindValue(":display_name"_L1, contentKind.displayName);
    updateQuery.bindValue(":is_active"_L1, contentKind.isActive ? 1 : 0);
    updateQuery.bindValue(":id"_L1, contentKind.id);
    return execWithError(updateQuery, errorMessage);
}

bool LookupsRepository::deleteContentKind(const QString &contentKindId, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("content_kind"), contentKindId.trimmed());
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content kind not found.");
        }
        return false;
    }

    const auto contentCount = lookupReferenceCount(QStringLiteral("content"),
                                                   QStringLiteral("kind_id"),
                                                   existing.id,
                                                   errorMessage);
    const auto burstTemplateCount = lookupReferenceCount(QStringLiteral("burst_template"),
                                                         QStringLiteral("kind_id"),
                                                         existing.id,
                                                         errorMessage);
    if (contentCount < 0 || burstTemplateCount < 0) {
        return false;
    }

    if (contentCount > 0 || burstTemplateCount > 0) {
        if (errorMessage != nullptr) {
            QStringList reasons;
            if (contentCount > 0) {
                reasons.append(QStringLiteral("%1 content items").arg(contentCount));
            }
            if (burstTemplateCount > 0) {
                reasons.append(QStringLiteral("%1 burst templates").arg(burstTemplateCount));
            }
            *errorMessage = QStringLiteral("Cannot delete content kind because it is still used by %1.")
                                .arg(reasons.join(QStringLiteral(", ")));
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery deleteContentKindQuery{database_};
    deleteContentKindQuery.prepare(QStringLiteral("DELETE FROM content_kind WHERE id = :id"));
    deleteContentKindQuery.bindValue(":id"_L1, existing.id);
    if (!execWithError(deleteContentKindQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery normalizeOrderQuery{database_};
    normalizeOrderQuery.prepare(QStringLiteral(
        "UPDATE content_kind "
        "SET sort_order = sort_order - 1 "
        "WHERE sort_order > :sort_order"));
    normalizeOrderQuery.bindValue(":sort_order"_L1, existing.sortOrder);
    if (!execWithError(normalizeOrderQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::reorderContentKinds(const QStringList &orderedIds, QString *errorMessage)
{
    const auto existingContentKinds = allLookups(QStringLiteral("content_kind"));
    if (orderedIds.size() != static_cast<qsizetype>(existingContentKinds.size())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content kind order is incomplete.");
        }
        return false;
    }

    QSet<QString> expectedIds;
    for (const auto &contentKind : existingContentKinds) {
        expectedIds.insert(contentKind.id);
    }
    QSet<QString> actualIds;
    for (const auto &id : orderedIds) {
        actualIds.insert(id);
    }
    if (actualIds != expectedIds) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content kind order must include each content kind exactly once.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral("UPDATE content_kind SET sort_order = :sort_order WHERE id = :id"));
    for (qsizetype index = 0; index < orderedIds.size(); ++index) {
        updateQuery.bindValue(":sort_order"_L1, static_cast<int>(index));
        updateQuery.bindValue(":id"_L1, orderedIds.at(index));
        if (!execWithError(updateQuery, errorMessage)) {
            database_.rollback();
            return false;
        }
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

QString LookupsRepository::createOutcome(const Domain::LookupValue &outcome, QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return {};
    }

    QSqlQuery shiftQuery{database_};
    shiftQuery.prepare(QStringLiteral("UPDATE outcome SET sort_order = sort_order + 1"));
    if (!execWithError(shiftQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery insertQuery{database_};
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO outcome (id, key, display_name, description, sort_order, is_active) "
        "VALUES (:id, :key, :display_name, :description, 0, :is_active)"));
    insertQuery.bindValue(":id"_L1, id);
    insertQuery.bindValue(":key"_L1, outcome.key);
    insertQuery.bindValue(":display_name"_L1, outcome.displayName);
    insertQuery.bindValue(":description"_L1, outcome.description);
    insertQuery.bindValue(":is_active"_L1, outcome.isActive ? 1 : 0);
    if (!execWithError(insertQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    if (database_.commit()) {
        return id;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return {};
}

bool LookupsRepository::updateOutcome(const Domain::LookupValue &outcome, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("outcome"), outcome.id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Outcome not found.");
        }
        return false;
    }

    if (existing.key != outcome.key && isRequiredManagedOutcomeKey(existing.key)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Cannot change this outcome key because the app depends on \"%1\" for built-in fan-out templates.")
                                .arg(existing.key);
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral(
        "UPDATE outcome "
        "SET key = :key, display_name = :display_name, is_active = :is_active "
        "WHERE id = :id"));
    updateQuery.bindValue(":key"_L1, outcome.key);
    updateQuery.bindValue(":display_name"_L1, outcome.displayName);
    updateQuery.bindValue(":is_active"_L1, outcome.isActive ? 1 : 0);
    updateQuery.bindValue(":id"_L1, outcome.id);
    return execWithError(updateQuery, errorMessage);
}

bool LookupsRepository::deleteOutcome(const QString &outcomeId, QString *errorMessage)
{
    const auto existing = lookupById(QStringLiteral("outcome"), outcomeId.trimmed());
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Outcome not found.");
        }
        return false;
    }

    const auto contentCount = lookupReferenceCount(QStringLiteral("content"),
                                                   QStringLiteral("outcome_id"),
                                                   existing.id,
                                                   errorMessage);
    const auto burstTemplateCount = lookupReferenceCount(QStringLiteral("burst_template"),
                                                         QStringLiteral("outcome_id"),
                                                         existing.id,
                                                         errorMessage);
    if (contentCount < 0 || burstTemplateCount < 0) {
        return false;
    }

    if (contentCount > 0 || burstTemplateCount > 0) {
        if (errorMessage != nullptr) {
            QStringList reasons;
            if (contentCount > 0) {
                reasons.append(QStringLiteral("%1 content items").arg(contentCount));
            }
            if (burstTemplateCount > 0) {
                reasons.append(QStringLiteral("%1 burst templates").arg(burstTemplateCount));
            }
            *errorMessage = QStringLiteral("Cannot delete outcome because it is still used by %1.")
                                .arg(reasons.join(QStringLiteral(", ")));
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery deleteOutcomeQuery{database_};
    deleteOutcomeQuery.prepare(QStringLiteral("DELETE FROM outcome WHERE id = :id"));
    deleteOutcomeQuery.bindValue(":id"_L1, existing.id);
    if (!execWithError(deleteOutcomeQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery normalizeOrderQuery{database_};
    normalizeOrderQuery.prepare(QStringLiteral(
        "UPDATE outcome "
        "SET sort_order = sort_order - 1 "
        "WHERE sort_order > :sort_order"));
    normalizeOrderQuery.bindValue(":sort_order"_L1, existing.sortOrder);
    if (!execWithError(normalizeOrderQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::reorderOutcomes(const QStringList &orderedIds, QString *errorMessage)
{
    const auto existingOutcomes = allLookups(QStringLiteral("outcome"));
    if (orderedIds.size() != static_cast<qsizetype>(existingOutcomes.size())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Outcome order is incomplete.");
        }
        return false;
    }

    QSet<QString> expectedIds;
    for (const auto &outcome : existingOutcomes) {
        expectedIds.insert(outcome.id);
    }
    QSet<QString> actualIds;
    for (const auto &id : orderedIds) {
        actualIds.insert(id);
    }
    if (actualIds != expectedIds) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Outcome order must include each outcome exactly once.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral("UPDATE outcome SET sort_order = :sort_order WHERE id = :id"));
    for (qsizetype index = 0; index < orderedIds.size(); ++index) {
        updateQuery.bindValue(":sort_order"_L1, static_cast<int>(index));
        updateQuery.bindValue(":id"_L1, orderedIds.at(index));
        if (!execWithError(updateQuery, errorMessage)) {
            database_.rollback();
            return false;
        }
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

QString LookupsRepository::createContentStatus(const Domain::ContentStatus &contentStatus,
                                               QString *errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return {};
    }

    QSqlQuery shiftQuery{database_};
    shiftQuery.prepare(QStringLiteral("UPDATE content_status SET sort_order = -(sort_order + 1)"));
    if (!execWithError(shiftQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    QSqlQuery normalizeShiftQuery{database_};
    normalizeShiftQuery.prepare(QStringLiteral("UPDATE content_status SET sort_order = ABS(sort_order)"));
    if (!execWithError(normalizeShiftQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    QSqlQuery insertQuery{database_};
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO content_status (id, info, sort_order, is_system) "
        "VALUES (:id, :info, 0, :is_system)"));
    insertQuery.bindValue(":id"_L1, contentStatus.id);
    insertQuery.bindValue(":info"_L1, contentStatus.info);
    insertQuery.bindValue(":is_system"_L1, contentStatus.isSystem ? 1 : 0);
    if (!execWithError(insertQuery, errorMessage)) {
        database_.rollback();
        return {};
    }

    if (database_.commit()) {
        return contentStatus.id;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return {};
}

bool LookupsRepository::updateContentStatus(const QString &existingId,
                                            const Domain::ContentStatus &contentStatus,
                                            QString *errorMessage)
{
    const auto existing = contentStatusById(existingId.trimmed());
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Status not found.");
        }
        return false;
    }

    if (existing.isSystem && existing.id != contentStatus.id) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot change the key of a system status.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    if (existing.id != contentStatus.id) {
        const auto contentCount = lookupReferenceCount(QStringLiteral("content"),
                                                       QStringLiteral("status"),
                                                       existing.id,
                                                       errorMessage);
        if (contentCount < 0) {
            database_.rollback();
            return false;
        }
        if (contentCount > 0) {
            database_.rollback();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral(
                    "Cannot change status key because %1 content items still use \"%2\".")
                                    .arg(contentCount)
                                    .arg(existing.id);
            }
            return false;
        }
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral(
        "UPDATE content_status "
        "SET id = :id, info = :info "
        "WHERE id = :existing_id"));
    updateQuery.bindValue(":id"_L1, contentStatus.id);
    updateQuery.bindValue(":info"_L1, contentStatus.info);
    updateQuery.bindValue(":existing_id"_L1, existing.id);
    if (!execWithError(updateQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::reorderContentStatuses(const QStringList &orderedIds, QString *errorMessage)
{
    const auto existingStatuses = contentStatuses();
    if (orderedIds.size() != static_cast<qsizetype>(existingStatuses.size())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Status order is incomplete.");
        }
        return false;
    }

    QSet<QString> expectedIds;
    for (const auto &status : existingStatuses) {
        expectedIds.insert(status.id);
    }
    QSet<QString> actualIds;
    for (const auto &id : orderedIds) {
        actualIds.insert(id);
    }
    if (actualIds != expectedIds) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Status order must include each status exactly once.");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery updateQuery{database_};
    updateQuery.prepare(QStringLiteral("UPDATE content_status SET sort_order = :sort_order WHERE id = :id"));
    for (qsizetype index = 0; index < orderedIds.size(); ++index) {
        updateQuery.bindValue(":sort_order"_L1, -static_cast<int>(index) - 1);
        updateQuery.bindValue(":id"_L1, orderedIds.at(index));
        if (!execWithError(updateQuery, errorMessage)) {
            database_.rollback();
            return false;
        }
    }

    QSqlQuery normalizeQuery{database_};
    normalizeQuery.prepare(QStringLiteral("UPDATE content_status SET sort_order = (-sort_order) - 1"));
    if (!execWithError(normalizeQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::deleteContentStatus(const QString &contentStatusId, QString *errorMessage)
{
    const auto existing = contentStatusById(contentStatusId.trimmed());
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Status not found.");
        }
        return false;
    }

    if (existing.isSystem) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot delete this status because it is a system status.");
        }
        return false;
    }

    const auto contentCount = lookupReferenceCount(QStringLiteral("content"),
                                                   QStringLiteral("status"),
                                                   existing.id,
                                                   errorMessage);
    if (contentCount < 0) {
        return false;
    }
    if (contentCount > 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot delete status because it is still used by %1 content items.")
                                .arg(contentCount);
        }
        return false;
    }

    if (!database_.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery deleteQuery{database_};
    deleteQuery.prepare(QStringLiteral("DELETE FROM content_status WHERE id = :id"));
    deleteQuery.bindValue(":id"_L1, existing.id);
    if (!execWithError(deleteQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery normalizeOrderQuery{database_};
    normalizeOrderQuery.prepare(QStringLiteral(
        "UPDATE content_status "
        "SET sort_order = -sort_order "
        "WHERE sort_order > :sort_order"));
    normalizeOrderQuery.bindValue(":sort_order"_L1, existing.sortOrder);
    if (!execWithError(normalizeOrderQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    QSqlQuery finalizeOrderQuery{database_};
    finalizeOrderQuery.prepare(QStringLiteral(
        "UPDATE content_status "
        "SET sort_order = (-sort_order) - 1 "
        "WHERE sort_order < 0"));
    if (!execWithError(finalizeOrderQuery, errorMessage)) {
        database_.rollback();
        return false;
    }

    if (database_.commit()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = database_.lastError().text();
    }
    return false;
}

bool LookupsRepository::syncChannelFanOutTemplates(QString *errorMessage)
{
    const auto shortPostKindId = lookupIdByKeyInternal(database_, QStringLiteral("content_kind"), QStringLiteral("short_post"), errorMessage);
    const auto clipKindId = lookupIdByKeyInternal(database_, QStringLiteral("content_kind"), QStringLiteral("clip"), errorMessage);
    const auto newsletterKindId = lookupIdByKeyInternal(database_, QStringLiteral("content_kind"), QStringLiteral("newsletter"), errorMessage);
    const auto blogPostKindId = lookupIdByKeyInternal(database_, QStringLiteral("content_kind"), QStringLiteral("blog_post"), errorMessage);
    const auto authorityOutcomeId = lookupIdByKeyInternal(database_, QStringLiteral("outcome"), QStringLiteral("authority"), errorMessage);
    const auto trustOutcomeId = lookupIdByKeyInternal(database_, QStringLiteral("outcome"), QStringLiteral("trust"), errorMessage);
    if (shortPostKindId.isEmpty() || clipKindId.isEmpty() || newsletterKindId.isEmpty() || blogPostKindId.isEmpty()
        || authorityOutcomeId.isEmpty() || trustOutcomeId.isEmpty()) {
        if (errorMessage != nullptr && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Missing seeded lookup values needed for channel fan-out templates.");
        }
        return false;
    }

    QSqlQuery deactivateQuery{database_};
    deactivateQuery.prepare(QStringLiteral("UPDATE burst_template SET is_active = 0"));
    if (!execWithError(deactivateQuery, errorMessage)) {
        return false;
    }

    QSqlQuery channelQuery{database_};
    channelQuery.prepare(QStringLiteral(
        "SELECT id, key, display_name, is_active "
        "FROM channel "
        "ORDER BY sort_order ASC, display_name COLLATE NOCASE ASC, id ASC"));
    if (!execWithError(channelQuery, errorMessage)) {
        return false;
    }

    while (channelQuery.next()) {
        const auto channelId = channelQuery.value(0).toString();
        const auto channelKey = channelQuery.value(1).toString();
        const auto channelDisplayName = channelQuery.value(2).toString();
        const auto isActive = channelQuery.value(3).toBool();
        const auto kindKey = defaultFanOutKindKey(channelKey);
        const auto outcomeKey = defaultFanOutOutcomeKey(channelKey);
        const auto kindId = kindKey == "clip"_L1 ? clipKindId
                           : kindKey == "newsletter"_L1 ? newsletterKindId
                           : kindKey == "blog_post"_L1  ? blogPostKindId
                                                        : shortPostKindId;
        const auto outcomeId = outcomeKey == "trust"_L1 ? trustOutcomeId : authorityOutcomeId;

        QSqlQuery upsertQuery{database_};
        upsertQuery.prepare(QStringLiteral(
            "INSERT INTO burst_template "
            "(key, display_name, title_suffix, kind_id, suggested_channel_id, outcome_id, is_active) "
            "VALUES (:key, :display_name, :title_suffix, :kind_id, :suggested_channel_id, :outcome_id, :is_active) "
            "ON CONFLICT(key) DO UPDATE SET "
            "display_name = excluded.display_name, "
            "title_suffix = excluded.title_suffix, "
            "kind_id = excluded.kind_id, "
            "suggested_channel_id = excluded.suggested_channel_id, "
            "outcome_id = excluded.outcome_id, "
            "is_active = excluded.is_active"));
        upsertQuery.bindValue(":key"_L1, managedFanOutTemplateKey(channelKey));
        upsertQuery.bindValue(":display_name"_L1, channelDisplayName);
        upsertQuery.bindValue(":title_suffix"_L1, QStringLiteral(" - %1").arg(channelDisplayName));
        upsertQuery.bindValue(":kind_id"_L1, kindId);
        upsertQuery.bindValue(":suggested_channel_id"_L1, channelId);
        upsertQuery.bindValue(":outcome_id"_L1, outcomeId);
        upsertQuery.bindValue(":is_active"_L1, isActive ? 1 : 0);
        if (!execWithError(upsertQuery, errorMessage)) {
            return false;
        }
    }

    return true;
}

int LookupsRepository::lookupReferenceCount(const QString &tableName,
                                            const QString &columnName,
                                            const QString &value,
                                            QString *errorMessage) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2 = :value").arg(tableName, columnName));
    query.bindValue(":value"_L1, value);
    if (!execWithError(query, errorMessage)) {
        return -1;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

} // namespace SmTool::Data
