#include "data/contentrepository.h"
#include "data/dashboardrepository.h"
#include "data/database.h"
#include "data/lookupsrepository.h"
#include "data/seriesrepository.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace Qt::Literals::StringLiterals;

namespace {

QString createTempDatabasePath()
{
    static QTemporaryDir tempDir;
    return QDir{tempDir.path()}.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
}

} // namespace

class DatabaseTests : public QObject
{
    Q_OBJECT

private slots:
    void createsSchemaAndSeedsDefaults();
    void enforcesForeignKeys();
    void createsSeriesAndContentRelationships();
    void burstGenerationIsIdempotent();
    void calendarUsesPublicationScheduleFallback();
};

void DatabaseTests::createsSchemaAndSeedsDefaults()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-schema"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    QSqlQuery query{database.connection()};
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM pillar")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 6);

    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM burst_template")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 5);
}

void DatabaseTests::enforcesForeignKeys()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-fk"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    QSqlQuery query{database.connection()};
    query.prepare(QStringLiteral(
        "INSERT INTO content "
        "(id, title, kind_id, pillar_id, status, priority, created_at, updated_at) "
        "VALUES ('content-1', 'Broken', 'missing-kind', 'missing-pillar', 'inbox', 0, datetime('now'), datetime('now'))"));
    QVERIFY(!query.exec());
}

void DatabaseTests::createsSeriesAndContentRelationships()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-relations"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto seriesRepository = SmTool::Data::SeriesRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("blog_post"));
    QVERIFY(!pillarId.isEmpty());
    QVERIFY(!kindId.isEmpty());

    const auto seriesId = seriesRepository.create(QStringLiteral("POC Series"),
                                                  QStringLiteral("Description"),
                                                  pillarId,
                                                  QStringLiteral("active"),
                                                  &errorMessage);
    QVERIFY2(!seriesId.isEmpty(), qPrintable(errorMessage));

    const auto sourceId = contentRepository.create({
        .seriesId = seriesId,
        .title = QStringLiteral("Root source"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("drafting"),
        .priority = 50,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!sourceId.isEmpty(), qPrintable(errorMessage));

    const auto roots = contentRepository.rootItems(false);
    QVERIFY(std::ranges::any_of(roots, [&](const auto &item) { return item.id == sourceId; }));
}

void DatabaseTests::burstGenerationIsIdempotent()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-burst"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("blog_post"));
    const auto sourceId = contentRepository.create({
        .title = QStringLiteral("Launch write-up"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("ready"),
        .priority = 80,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!sourceId.isEmpty(), qPrintable(errorMessage));

    QVERIFY2(contentRepository.createBurst(sourceId, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(contentRepository.createBurst(sourceId, &errorMessage), qPrintable(errorMessage));

    const auto children = contentRepository.childItems(sourceId);
    QCOMPARE(static_cast<int>(children.size()), 5);

    QSqlQuery query{database.connection()};
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM content WHERE parent_id IS NOT NULL")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 5);
}

void DatabaseTests::calendarUsesPublicationScheduleFallback()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-calendar"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};
    auto dashboardRepository = SmTool::Data::DashboardRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("video"));
    const auto channelId = lookups.lookupIdByKey(QStringLiteral("channel"), QStringLiteral("youtube"));

    const auto scheduledAt = QDateTime::currentDateTimeUtc().addDays(3);
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Calendar item"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("scheduled"),
        .priority = 60,
        .scheduledAt = scheduledAt,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    QSqlQuery insertPublication{database.connection()};
    insertPublication.prepare(QStringLiteral(
        "INSERT INTO publication "
        "(id, content_id, channel_id, status, scheduled_at, published_at, url, created_at, updated_at) "
        "VALUES (:id, :content_id, :channel_id, 'planned', NULL, NULL, '', datetime('now'), datetime('now'))"));
    insertPublication.bindValue(":id"_L1, QStringLiteral("pub-1"));
    insertPublication.bindValue(":content_id"_L1, contentId);
    insertPublication.bindValue(":channel_id"_L1, channelId);
    QVERIFY(insertPublication.exec());

    const auto entries = dashboardRepository.calendarEntries();
    QVERIFY(std::ranges::any_of(entries, [&](const auto &entry) {
        return entry.contentId == contentId
            && entry.sourceType == "publication"
            && entry.scheduledAt.toUTC().toSecsSinceEpoch() == scheduledAt.toUTC().toSecsSinceEpoch();
    }));
}

QTEST_MAIN(DatabaseTests)

#include "test_database.moc"
