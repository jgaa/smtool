#include "data/mediarepository.h"

#include "app/loggingcontroller.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

QVariant nullableString(const QString &value)
{
    return value.trimmed().isEmpty() ? QVariant{} : QVariant{value.trimmed()};
}

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

QString quotedSqlString(QString value)
{
    value.replace(u'\'', QStringLiteral("''"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

} // namespace

MediaRepository::MediaRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

std::vector<Domain::MediaItem> MediaRepository::listForContent(const QString &contentId) const
{
    return listForOwner(QStringLiteral("content_id"), contentId);
}

std::vector<Domain::MediaItem> MediaRepository::listForPublication(const QString &publicationId) const
{
    return listForOwner(QStringLiteral("publication_id"), publicationId);
}

bool MediaRepository::replaceForContent(const QString &contentId,
                                        const std::vector<Domain::MediaItem> &items,
                                        QString *errorMessage) const
{
    return replaceForOwner(QStringLiteral("content_id"), contentId, items, errorMessage);
}

bool MediaRepository::replaceForPublication(const QString &publicationId,
                                            const std::vector<Domain::MediaItem> &items,
                                            QString *errorMessage) const
{
    return replaceForOwner(QStringLiteral("publication_id"), publicationId, items, errorMessage);
}

std::vector<Domain::MediaItem> MediaRepository::listForOwner(const QString &ownerColumn, const QString &ownerId) const
{
    QSqlQuery query{database_};
    const auto statement = QStringLiteral(
        "SELECT id, COALESCE(content_id, ''), COALESCE(publication_id, ''), name, source_type, location, created_at, updated_at "
        "FROM media "
        "WHERE %1 = %2 "
        "ORDER BY created_at ASC, name ASC").arg(ownerColumn, quotedSqlString(ownerId));
    LOG_DEBUG << "MediaRepository::listForOwner SQL: " << statement.toStdString();
    query.exec(statement);

    std::vector<Domain::MediaItem> items;
    while (query.next()) {
        items.push_back({
            .id = query.value(0).toString(),
            .contentId = query.value(1).toString(),
            .publicationId = query.value(2).toString(),
            .name = query.value(3).toString(),
            .sourceType = query.value(4).toString(),
            .location = query.value(5).toString(),
            .createdAt = query.value(6).toDateTime(),
            .updatedAt = query.value(7).toDateTime(),
        });
    }
    query.finish();
    LOG_TRACE << "MediaRepository::listForOwner completed ownerColumn='"
              << ownerColumn.toStdString() << "' ownerId='"
              << ownerId.toStdString() << "' items=" << items.size();
    return items;
}

bool MediaRepository::replaceForOwner(const QString &ownerColumn,
                                      const QString &ownerId,
                                      const std::vector<Domain::MediaItem> &items,
                                      QString *errorMessage) const
{
    LOG_DEBUG << "MediaRepository::replaceForOwner ownerColumn='" << ownerColumn.toStdString()
              << "' ownerId='" << ownerId.toStdString()
              << "' items=" << items.size();
    QSqlQuery deleteQuery{database_};
    const auto deleteStatement = QStringLiteral("DELETE FROM media WHERE %1 = %2")
                                     .arg(ownerColumn, quotedSqlString(ownerId));
    LOG_DEBUG << "MediaRepository::replaceForOwner delete SQL: " << deleteStatement.toStdString();
    deleteQuery.prepare(deleteStatement);
    if (!execWithError(deleteQuery, errorMessage)) {
        LOG_ERROR << "MediaRepository::replaceForOwner delete failed for ownerId='"
                  << ownerId.toStdString() << "': "
                  << (errorMessage != nullptr ? errorMessage->toStdString() : std::string{});
        return false;
    }
    deleteQuery.finish();
    LOG_TRACE << "MediaRepository::replaceForOwner delete finished for ownerId='"
              << ownerId.toStdString() << "'";

    for (const auto &item : items) {
        LOG_DEBUG << "MediaRepository::replaceForOwner inserting media id='" << item.id.toStdString()
                  << "' sourceType='" << item.sourceType.toStdString()
                  << "' location='" << item.location.toStdString() << "'";
        QSqlQuery insertQuery{database_};
        insertQuery.prepare(QStringLiteral(
            "INSERT INTO media (id, content_id, publication_id, name, source_type, location, created_at, updated_at) "
            "VALUES (:id, :content_id, :publication_id, :name, :source_type, :location, :created_at, :updated_at)"));
        const auto id = item.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : item.id;
        const auto timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        insertQuery.bindValue(":id"_L1, id);
        insertQuery.bindValue(":content_id"_L1, ownerColumn == "content_id"_L1 ? ownerId : QVariant{});
        insertQuery.bindValue(":publication_id"_L1, ownerColumn == "publication_id"_L1 ? ownerId : QVariant{});
        insertQuery.bindValue(":name"_L1, item.name.trimmed());
        insertQuery.bindValue(":source_type"_L1, item.sourceType.trimmed());
        insertQuery.bindValue(":location"_L1, item.location.trimmed());
        insertQuery.bindValue(":created_at"_L1, item.createdAt.isValid() ? item.createdAt.toString(Qt::ISODate) : timestamp);
        insertQuery.bindValue(":updated_at"_L1, timestamp);
        if (!execWithError(insertQuery, errorMessage)) {
            LOG_ERROR << "MediaRepository::replaceForOwner insert failed for ownerId='"
                      << ownerId.toStdString() << "': "
                      << (errorMessage != nullptr ? errorMessage->toStdString() : std::string{});
            return false;
        }
        insertQuery.finish();
        LOG_TRACE << "MediaRepository::replaceForOwner insert finished mediaId='"
                  << id.toStdString() << "' ownerId='"
                  << ownerId.toStdString() << "'";
    }

    LOG_DEBUG << "MediaRepository::replaceForOwner completed for ownerId='" << ownerId.toStdString() << "'";
    return true;
}

} // namespace SmTool::Data
