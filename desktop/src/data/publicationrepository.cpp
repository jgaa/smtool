#include "data/publicationrepository.h"

#include "domain/constants.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QUuid>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

QVariant nullableString(const QString &value)
{
    return value.trimmed().isEmpty() ? QVariant{} : QVariant{value.trimmed()};
}

QVariant nullableDateTime(const QDateTime &value)
{
    return value.isValid() ? QVariant{value.toString(Qt::ISODate)} : QVariant{};
}

} // namespace

PublicationRepository::PublicationRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::Publication> PublicationRepository::listForContent(const QString &contentId) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT p.id, p.content_id, p.channel_id, COALESCE(ch.display_name, ''), p.status, p.scheduled_at, p.published_at, "
        "COALESCE(p.url, ''), p.created_at, p.updated_at "
        "FROM publication p "
        "JOIN channel ch ON ch.id = p.channel_id "
        "WHERE p.content_id = :content_id "
        "ORDER BY COALESCE(p.scheduled_at, p.created_at) ASC, p.created_at ASC"));
    query.bindValue(":content_id"_L1, contentId);
    query.exec();

    std::vector<Domain::Publication> publications;
    while (query.next()) {
        publications.push_back({
            .id = query.value(0).toString(),
            .contentId = query.value(1).toString(),
            .channelId = query.value(2).toString(),
            .channelName = query.value(3).toString(),
            .status = query.value(4).toString(),
            .scheduledAt = query.value(5).toDateTime(),
            .publishedAt = query.value(6).toDateTime(),
            .url = query.value(7).toString(),
            .createdAt = query.value(8).toDateTime(),
            .updatedAt = query.value(9).toDateTime(),
        });
    }
    return publications;
}

Domain::Publication PublicationRepository::getById(const QString &id) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "SELECT p.id, p.content_id, p.channel_id, COALESCE(ch.display_name, ''), p.status, p.scheduled_at, p.published_at, "
        "COALESCE(p.url, ''), p.created_at, p.updated_at "
        "FROM publication p "
        "JOIN channel ch ON ch.id = p.channel_id "
        "WHERE p.id = :id"));
    query.bindValue(":id"_L1, id);
    query.exec();
    if (!query.next()) {
        return {};
    }

    return {
        .id = query.value(0).toString(),
        .contentId = query.value(1).toString(),
        .channelId = query.value(2).toString(),
        .channelName = query.value(3).toString(),
        .status = query.value(4).toString(),
        .scheduledAt = query.value(5).toDateTime(),
        .publishedAt = query.value(6).toDateTime(),
        .url = query.value(7).toString(),
        .createdAt = query.value(8).toDateTime(),
        .updatedAt = query.value(9).toDateTime(),
    };
}

QString PublicationRepository::create(const Domain::Publication &publication, QString *errorMessage) const
{
    if (!Domain::isValidPublicationStatus(publication.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid publication status: %1").arg(publication.status);
        }
        return {};
    }

    const auto id = publication.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : publication.id;
    const auto now = QDateTime::currentDateTimeUtc();

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "INSERT INTO publication (id, content_id, channel_id, status, scheduled_at, published_at, url, created_at, updated_at) "
        "VALUES (:id, :content_id, :channel_id, :status, :scheduled_at, :published_at, :url, :created_at, :updated_at)"));
    query.bindValue(":id"_L1, id);
    query.bindValue(":content_id"_L1, publication.contentId);
    query.bindValue(":channel_id"_L1, publication.channelId);
    query.bindValue(":status"_L1, publication.status);
    query.bindValue(":scheduled_at"_L1, nullableDateTime(publication.scheduledAt));
    query.bindValue(":published_at"_L1, nullableDateTime(publication.publishedAt));
    query.bindValue(":url"_L1, nullableString(publication.url));
    query.bindValue(":created_at"_L1, now.toString(Qt::ISODate));
    query.bindValue(":updated_at"_L1, now.toString(Qt::ISODate));
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }

    return id;
}

bool PublicationRepository::createMissingForContent(const QString &contentId,
                                                    const QStringList &channelIds,
                                                    int *createdCount,
                                                    QString *errorMessage) const
{
    const auto trimmedContentId = contentId.trimmed();
    if (trimmedContentId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Content id is required.");
        }
        return false;
    }

    QStringList requestedChannels;
    requestedChannels.reserve(channelIds.size());
    for (const auto &channelId : channelIds) {
        const auto trimmedChannelId = channelId.trimmed();
        if (!trimmedChannelId.isEmpty()) {
            requestedChannels.push_back(trimmedChannelId);
        }
    }
    requestedChannels.removeDuplicates();

    if (requestedChannels.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Select at least one publication channel.");
        }
        return false;
    }

    QSqlQuery existingQuery{database_};
    existingQuery.prepare(QStringLiteral("SELECT channel_id FROM publication WHERE content_id = :content_id"));
    existingQuery.bindValue(":content_id"_L1, trimmedContentId);
    if (!existingQuery.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = existingQuery.lastError().text();
        }
        return false;
    }

    QSet<QString> existingChannels;
    while (existingQuery.next()) {
        existingChannels.insert(existingQuery.value(0).toString());
    }

    int created = 0;
    for (const auto &channelId : requestedChannels) {
        if (existingChannels.contains(channelId)) {
            continue;
        }

        if (create({
                       .contentId = trimmedContentId,
                       .channelId = channelId,
                       .status = QStringLiteral("planned"),
                   },
                   errorMessage)
                .isEmpty()) {
            return false;
        }

        existingChannels.insert(channelId);
        ++created;
    }

    if (createdCount != nullptr) {
        *createdCount = created;
    }
    return true;
}

bool PublicationRepository::update(const Domain::Publication &publication, QString *errorMessage) const
{
    if (publication.id.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Publication id is required");
        }
        return false;
    }
    if (!Domain::isValidPublicationStatus(publication.status)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid publication status: %1").arg(publication.status);
        }
        return false;
    }

    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE publication SET "
        "channel_id = :channel_id, "
        "status = :status, "
        "scheduled_at = :scheduled_at, "
        "published_at = :published_at, "
        "url = :url, "
        "updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(":id"_L1, publication.id);
    query.bindValue(":channel_id"_L1, publication.channelId);
    query.bindValue(":status"_L1, publication.status);
    query.bindValue(":scheduled_at"_L1, nullableDateTime(publication.scheduledAt));
    query.bindValue(":published_at"_L1, nullableDateTime(publication.publishedAt));
    query.bindValue(":url"_L1, nullableString(publication.url));
    query.bindValue(":updated_at"_L1, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (query.exec()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool PublicationRepository::updateScheduledAtForContentOnDate(const QString &contentId,
                                                              const QDate &fromDate,
                                                              const QDateTime &scheduledAt,
                                                              QString *errorMessage) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral(
        "UPDATE publication SET scheduled_at = :scheduled_at, updated_at = :updated_at "
        "WHERE content_id = :content_id AND date(scheduled_at) = :from_date"));
    query.bindValue(":scheduled_at"_L1, scheduledAt.toString(Qt::ISODate));
    query.bindValue(":updated_at"_L1, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    query.bindValue(":content_id"_L1, contentId);
    query.bindValue(":from_date"_L1, fromDate.toString(Qt::ISODate));
    if (query.exec()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool PublicationRepository::remove(const QString &id, QString *errorMessage) const
{
    QSqlQuery query{database_};
    query.prepare(QStringLiteral("DELETE FROM publication WHERE id = :id"));
    query.bindValue(":id"_L1, id);
    if (query.exec()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

} // namespace SmTool::Data
