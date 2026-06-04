#include "data/contentrepository.h"

#include "domain/constants.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <ranges>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QVariant nullableString(const QString &value)
{
    return value.isEmpty() ? QVariant{} : QVariant{value};
}

QVariant nullableDateTime(const QDateTime &value)
{
    return value.isValid() ? QVariant{value.toString(Qt::ISODate)} : QVariant{};
}

bool removeContentTree(QSqlDatabase &db, const QString &id, QString *errorMessage)
{
    QSqlQuery childQuery{db};
    childQuery.prepare(QStringLiteral("SELECT id FROM content WHERE parent_id = :parent_id"));
    childQuery.bindValue(":parent_id"_L1, id);
    if (!childQuery.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = childQuery.lastError().text();
        }
        return false;
    }

    while (childQuery.next()) {
        if (!removeContentTree(db, childQuery.value(0).toString(), errorMessage)) {
            return false;
        }
    }

    for (const auto &statement : {
             QStringLiteral("DELETE FROM publication WHERE content_id = :id"),
             QStringLiteral("DELETE FROM note WHERE content_id = :id"),
             QStringLiteral("DELETE FROM content WHERE id = :id"),
         }) {
        QSqlQuery query{db};
        query.prepare(statement);
        query.bindValue(":id"_L1, id);
        if (!query.exec()) {
            if (errorMessage != nullptr) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    return true;
}

QString selectSummaryBase()
{
    return QStringLiteral(
        "SELECT c.id, COALESCE(c.parent_id, ''), COALESCE(c.burst_template_key, ''), c.title, COALESCE(c.description, ''), "
        "COALESCE(p.display_name, ''), COALESCE(s.name, ''), COALESCE(k.display_name, ''), COALESCE(o.display_name, ''), "
        "COALESCE(ch.display_name, ''), c.status, c.priority, c.scheduled_at, c.published_at "
        "FROM content c "
        "LEFT JOIN pillar p ON p.id = c.pillar_id "
        "LEFT JOIN series s ON s.id = c.series_id "
        "LEFT JOIN content_kind k ON k.id = c.kind_id "
        "LEFT JOIN outcome o ON o.id = c.outcome_id "
        "LEFT JOIN channel ch ON ch.id = c.suggested_channel_id ");
}

} // namespace

ContentRepository::ContentRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::ContentSummary> ContentRepository::inboxItems() const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.status = 'inbox' "
                      "ORDER BY c.priority DESC, c.created_at DESC"));
    query.exec();
    return runSummaryQuery(query);
}

std::vector<Domain::ContentSummary> ContentRepository::boardItems(const QString &status, bool includeArchived) const
{
    if (!Domain::isValidContentStatus(status)) {
        return {};
    }

    QSqlQuery query{database_};
    if (status == "archived"_L1) {
        query.prepare(selectSummaryBase() + QStringLiteral(
                          "WHERE c.status = 'archived' AND :include_archived = 1 "
                          "ORDER BY c.priority DESC, c.updated_at DESC"));
        query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    } else {
        query.prepare(selectSummaryBase() + QStringLiteral(
                          "WHERE c.status = :status "
                          "ORDER BY c.priority DESC, c.updated_at DESC"));
        query.bindValue(":status"_L1, status);
    }
    query.exec();
    return runSummaryQuery(query);
}

std::vector<Domain::ContentSummary> ContentRepository::rootItems(bool includeArchived) const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.parent_id IS NULL AND (:include_archived = 1 OR c.status != 'archived') "
                      "ORDER BY c.updated_at DESC, c.title ASC"));
    query.bindValue(":include_archived"_L1, includeArchived ? 1 : 0);
    query.exec();
    return runSummaryQuery(query);
}

std::vector<Domain::ContentSummary> ContentRepository::childItems(const QString &parentId) const
{
    QSqlQuery query{database_};
    query.prepare(selectSummaryBase() + QStringLiteral(
                      "WHERE c.parent_id = :parent_id "
                      "ORDER BY c.created_at ASC"));
    query.bindValue(":parent_id"_L1, parentId);
    query.exec();
    return runSummaryQuery(query);
}

std::vector<Domain::BurstTemplate> ContentRepository::activeBurstTemplates() const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT key, display_name, title_suffix, kind_id, COALESCE(suggested_channel_id, ''), COALESCE(outcome_id, '') "
        "FROM burst_template "
        "WHERE is_active = 1 "
        "ORDER BY display_name ASC"));
    query.exec();

    std::vector<Domain::BurstTemplate> results;
    while (query.next()) {
        results.push_back({
            .key = query.value(0).toString(),
            .displayName = query.value(1).toString(),
            .titleSuffix = query.value(2).toString(),
            .kindId = query.value(3).toString(),
            .suggestedChannelId = query.value(4).toString(),
            .outcomeId = query.value(5).toString(),
        });
    }
    return results;
}

QString ContentRepository::create(const Domain::ContentItem &content, QString *errorMessage) const
{
    if (!Domain::isValidContentStatus(content.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid content status: %1").arg(content.status);
        }
        return {};
    }
    if (content.priority < 0 || content.priority > 100) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Priority out of range");
        }
        return {};
    }

    const auto id = content.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : content.id;
    const auto createdAt = content.createdAt.isValid() ? content.createdAt : QDateTime::currentDateTimeUtc();
    const auto updatedAt = content.updatedAt.isValid() ? content.updatedAt : createdAt;
    const auto publishedAt = content.status == "published"_L1 && !content.publishedAt.isValid()
        ? updatedAt
        : content.publishedAt;

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO content "
        "(id, parent_id, series_id, burst_template_key, title, description, kind_id, pillar_id, outcome_id, suggested_channel_id, status, priority, scheduled_at, published_at, published_url, created_at, updated_at) "
        "VALUES "
        "(:id, :parent_id, :series_id, :burst_template_key, :title, :description, :kind_id, :pillar_id, :outcome_id, :suggested_channel_id, :status, :priority, :scheduled_at, :published_at, :published_url, :created_at, :updated_at)"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":parent_id"_L1, nullableString(content.parentId));
    query.bindValue(":series_id"_L1, nullableString(content.seriesId));
    query.bindValue(":burst_template_key"_L1, nullableString(content.burstTemplateKey));
    query.bindValue(":title"_L1, content.title);
    query.bindValue(":description"_L1, nullableString(content.description));
    query.bindValue(":kind_id"_L1, content.kindId);
    query.bindValue(":pillar_id"_L1, content.pillarId);
    query.bindValue(":outcome_id"_L1, nullableString(content.outcomeId));
    query.bindValue(":suggested_channel_id"_L1, nullableString(content.suggestedChannelId));
    query.bindValue(":status"_L1, content.status);
    query.bindValue(":priority"_L1, content.priority);
    query.bindValue(":scheduled_at"_L1, nullableDateTime(content.scheduledAt));
    query.bindValue(":published_at"_L1, nullableDateTime(publishedAt));
    query.bindValue(":published_url"_L1, nullableString(content.publishedUrl));
    query.bindValue(":created_at"_L1, createdAt.toString(Qt::ISODate));
    query.bindValue(":updated_at"_L1, updatedAt.toString(Qt::ISODate));
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }

    return id;
}

bool ContentRepository::update(const Domain::ContentItem &content, QString *errorMessage) const
{
    if (content.id.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content id is required");
        }
        return false;
    }
    if (!Domain::isValidContentStatus(content.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid content status: %1").arg(content.status);
        }
        return false;
    }
    if (content.title.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Title is required");
        }
        return false;
    }
    if (content.priority < 0 || content.priority > 100) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Priority out of range");
        }
        return false;
    }

    const auto existing = getContentById(content.id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    const auto updatedAt = QDateTime::currentDateTimeUtc();
    const auto publishedAt = content.status == "published"_L1 && !existing.publishedAt.isValid()
        ? updatedAt
        : existing.publishedAt;

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE content SET "
        "title = :title, "
        "description = :description, "
        "pillar_id = :pillar_id, "
        "suggested_channel_id = :suggested_channel_id, "
        "status = :status, "
        "priority = :priority, "
        "scheduled_at = :scheduled_at, "
        "published_at = :published_at, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(":id"_L1, content.id);
    query.bindValue(":title"_L1, content.title.trimmed());
    query.bindValue(":description"_L1, nullableString(content.description.trimmed()));
    query.bindValue(":pillar_id"_L1, content.pillarId);
    query.bindValue(":suggested_channel_id"_L1, nullableString(content.suggestedChannelId));
    query.bindValue(":status"_L1, content.status);
    query.bindValue(":priority"_L1, content.priority);
    query.bindValue(":scheduled_at"_L1, nullableDateTime(content.scheduledAt));
    query.bindValue(":published_at"_L1, nullableDateTime(publishedAt));
    query.bindValue(":updated_at"_L1, updatedAt.toString(Qt::ISODate));
    if (query.exec()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool ContentRepository::updateStatus(const QString &id, const QString &newStatus, QString *errorMessage) const
{
    const auto existing = getContentById(id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    if (!Domain::isValidContentStatus(newStatus)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid content status: %1").arg(newStatus);
        }
        return false;
    }

    const auto now = QDateTime::currentDateTimeUtc();
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE content "
        "SET status = :status, "
        "updated_at = :updated_at, "
        "published_at = CASE "
        "    WHEN :status = 'published' AND published_at IS NULL THEN :updated_at "
        "    ELSE published_at "
        "END "
        "WHERE id = :id"));
    query.bindValue(":status"_L1, newStatus);
    query.bindValue(":updated_at"_L1, now.toString(Qt::ISODate));
    query.bindValue(":id"_L1, id);
    if (query.exec()) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool ContentRepository::remove(const QString &id, QString *errorMessage) const
{
    if (id.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content id is required");
        }
        return false;
    }

    const auto existing = getContentById(id);
    if (existing.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content not found");
        }
        return false;
    }

    auto db = database_;
    if (!db.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    if (!removeContentTree(db, id, errorMessage)) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool ContentRepository::createBurst(const QString &sourceContentId, QString *errorMessage) const
{
    QStringList templateKeys;
    for (const auto &burstTemplate : activeBurstTemplates()) {
        templateKeys.append(burstTemplate.key);
    }
    return createBurst(sourceContentId, templateKeys, errorMessage);
}

bool ContentRepository::createBurst(const QString &sourceContentId,
                                    const QStringList &templateKeys,
                                    QString *errorMessage) const
{
    const auto source = getContentById(sourceContentId);
    if (source.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Source content not found");
        }
        return false;
    }
    if (!source.parentId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Only root content may generate bursts");
        }
        return false;
    }
    if (templateKeys.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Select at least one burst alternative");
        }
        return false;
    }

    auto db = database_;
    if (!db.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    QSqlQuery templateQuery{db};
    for (const auto &burstTemplateKey : templateKeys) {
        templateQuery.prepare(QStringLiteral(
            "SELECT bt.key, bt.title_suffix, bt.kind_id, bt.outcome_id, bt.suggested_channel_id "
            "FROM burst_template bt "
            "WHERE bt.key = :key AND bt.is_active = 1"));
        templateQuery.bindValue(":key"_L1, burstTemplateKey);
        if (!templateQuery.exec()) {
            if (errorMessage != nullptr) {
                *errorMessage = templateQuery.lastError().text();
            }
            db.rollback();
            return false;
        }
        if (!templateQuery.next()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Burst alternative not found: %1").arg(burstTemplateKey);
            }
            db.rollback();
            return false;
        }

        QSqlQuery existingQuery{db};
        existingQuery.prepare(QStringLiteral(
            "SELECT id FROM content "
            "WHERE parent_id = :parent_id AND burst_template_key = :burst_template_key"));
        existingQuery.bindValue(":parent_id"_L1, source.id);
        existingQuery.bindValue(":burst_template_key"_L1, burstTemplateKey);
        if (!existingQuery.exec()) {
            if (errorMessage != nullptr) {
                *errorMessage = existingQuery.lastError().text();
            }
            db.rollback();
            return false;
        }
        if (existingQuery.next()) {
            continue;
        }

        Domain::ContentItem child{
            .parentId = source.id,
            .seriesId = source.seriesId,
            .burstTemplateKey = burstTemplateKey,
            .title = source.title + templateQuery.value(1).toString(),
            .description = source.description,
            .kindId = templateQuery.value(2).toString(),
            .pillarId = source.pillarId,
            .outcomeId = templateQuery.value(3).toString(),
            .suggestedChannelId = templateQuery.value(4).toString(),
            .status = QStringLiteral("shaping"),
            .priority = source.priority,
            .createdAt = QDateTime::currentDateTimeUtc(),
            .updatedAt = QDateTime::currentDateTimeUtc(),
        };
        if (create(child, errorMessage).isEmpty()) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

Domain::ContentItem ContentRepository::getById(const QString &id) const
{
    return getContentById(id);
}

std::vector<Domain::ContentSummary> ContentRepository::runSummaryQuery(QSqlQuery &query) const
{
    std::vector<Domain::ContentSummary> results;
    while (query.next()) {
        results.push_back({
            .id = query.value(0).toString(),
            .parentId = query.value(1).toString(),
            .burstTemplateKey = query.value(2).toString(),
            .title = query.value(3).toString(),
            .description = query.value(4).toString(),
            .pillarName = query.value(5).toString(),
            .seriesName = query.value(6).toString(),
            .kindName = query.value(7).toString(),
            .outcomeName = query.value(8).toString(),
            .suggestedChannelName = query.value(9).toString(),
            .status = query.value(10).toString(),
            .priority = query.value(11).toInt(),
            .scheduledAt = query.value(12).toDateTime(),
            .publishedAt = query.value(13).toDateTime(),
        });
    }
    return results;
}

Domain::ContentItem ContentRepository::getContentById(const QString &id) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT id, COALESCE(parent_id, ''), COALESCE(series_id, ''), COALESCE(burst_template_key, ''), title, "
        "COALESCE(description, ''), kind_id, pillar_id, COALESCE(outcome_id, ''), COALESCE(suggested_channel_id, ''), "
        "status, priority, scheduled_at, published_at, COALESCE(published_url, ''), created_at, updated_at "
        "FROM content WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    query.exec();
    if (!query.next()) {
        return {};
    }
    return {
        .id = query.value(0).toString(),
        .parentId = query.value(1).toString(),
        .seriesId = query.value(2).toString(),
        .burstTemplateKey = query.value(3).toString(),
        .title = query.value(4).toString(),
        .description = query.value(5).toString(),
        .kindId = query.value(6).toString(),
        .pillarId = query.value(7).toString(),
        .outcomeId = query.value(8).toString(),
        .suggestedChannelId = query.value(9).toString(),
        .status = query.value(10).toString(),
        .priority = query.value(11).toInt(),
        .scheduledAt = query.value(12).toDateTime(),
        .publishedAt = query.value(13).toDateTime(),
        .publishedUrl = query.value(14).toString(),
        .createdAt = query.value(15).toDateTime(),
        .updatedAt = query.value(16).toDateTime(),
    };
}

} // namespace SmTool::Data
