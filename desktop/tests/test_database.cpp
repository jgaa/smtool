#include "app/appcontroller.h"
#include "models/contentlistmodel.h"
#include "data/contentrepository.h"
#include "data/dashboardrepository.h"
#include "data/database.h"
#include "data/lookupsrepository.h"
#include "data/mediarepository.h"
#include "data/publicationrepository.h"
#include "data/seriesrepository.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
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

QString writeTempTextFile(const QString &suffix, const QString &content)
{
    const auto path = QDir{QFileInfo{createTempDatabasePath()}.absolutePath()}
                          .filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + suffix);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(content.toUtf8());
    file.close();
    return path;
}

} // namespace

class DatabaseTests : public QObject
{
    Q_OBJECT

private slots:
    void createsSchemaAndSeedsDefaults();
    void seedsContentStatuses();
    void enforcesForeignKeys();
    void createsSeriesAndContentRelationships();
    void burstGenerationIsIdempotent();
    void burstGenerationSupportsSelectedAlternatives();
    void calendarShowsContentAndPublicationSchedules();
    void calendarFiltersArchivedAndPublishedAndMarksOverdue();
    void reopensDatabaseAtNewPath();
    void createsIdeaFromPlainText();
    void createsIdeaFromMarkdown();
    void importsIdeaFromPlainTextFile();
    void importsIdeasFromMarkdownFileSections();
    void allowsArbitraryStatusTransition();
    void preventsDeletingSystemOrReferencedStatuses();
    void deletesContentTree();
    void updatesContentFields();
    void normalizesAndCachesTags();
    void sortsAllContentByDueDateThenTitle();
    void searchesContentByTextAndTags();
    void searchesSeriesByNameAndDescription();
    void contentModelBuildsDescriptionPreview();
    void publicationCrudWorks();
    void persistsMediaForContentAndPublication();
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

    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM channel")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 10);
}

void DatabaseTests::seedsContentStatuses()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-content-statuses"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    QSqlQuery query{database.connection()};
    QVERIFY(query.exec(QStringLiteral("SELECT id, sort_order, is_system, info FROM content_status ORDER BY sort_order ASC")));

    QStringList actualIds;
    while (query.next()) {
        actualIds.append(query.value(0).toString());
        const auto id = query.value(0).toString();
        const auto info = query.value(3).toString();
        QVERIFY(!info.isEmpty());
        QVERIFY(info.startsWith(QStringLiteral("## Workflow States")));
        if (id == "inbox"_L1 || id == "archived"_L1) {
            QVERIFY(query.value(2).toBool());
        }
    }

    QCOMPARE(actualIds, QStringList({
        QStringLiteral("inbox"),
        QStringLiteral("clarifying"),
        QStringLiteral("shaping"),
        QStringLiteral("drafting"),
        QStringLiteral("ready"),
        QStringLiteral("scheduled"),
        QStringLiteral("published"),
        QStringLiteral("reviewing"),
        QStringLiteral("archived"),
    }));
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

void DatabaseTests::burstGenerationSupportsSelectedAlternatives()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-burst-selected"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("blog_post"));
    const auto sourceId = contentRepository.create({
        .title = QStringLiteral("Selected burst write-up"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("ready"),
        .priority = 80,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!sourceId.isEmpty(), qPrintable(errorMessage));

    const QStringList selectedKeys{
        QStringLiteral("newsletter_summary"),
        QStringLiteral("short_video_excerpt"),
    };
    QVERIFY2(contentRepository.createBurst(sourceId, selectedKeys, &errorMessage), qPrintable(errorMessage));
    QVERIFY2(contentRepository.createBurst(sourceId, selectedKeys, &errorMessage), qPrintable(errorMessage));

    const auto children = contentRepository.childItems(sourceId);
    QCOMPARE(static_cast<int>(children.size()), 2);

    QStringList actualKeys;
    for (const auto &child : children) {
        actualKeys.append(child.burstTemplateKey);
    }
    std::sort(actualKeys.begin(), actualKeys.end());

    auto expectedKeys = selectedKeys;
    std::sort(expectedKeys.begin(), expectedKeys.end());
    QCOMPARE(actualKeys, expectedKeys);
}

void DatabaseTests::calendarShowsContentAndPublicationSchedules()
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

    const auto contentScheduledAt = QDateTime::currentDateTimeUtc().addDays(3);
    const auto publicationScheduledAt = QDateTime::currentDateTimeUtc().addDays(5);
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Calendar item"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("scheduled"),
        .priority = 60,
        .scheduledAt = contentScheduledAt,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    QSqlQuery insertPublication{database.connection()};
    insertPublication.prepare(QStringLiteral(
        "INSERT INTO publication "
        "(id, content_id, channel_id, status, scheduled_at, published_at, url, created_at, updated_at) "
        "VALUES (:id, :content_id, :channel_id, 'planned', :scheduled_at, NULL, '', datetime('now'), datetime('now'))"));
    insertPublication.bindValue(":id"_L1, QStringLiteral("pub-1"));
    insertPublication.bindValue(":content_id"_L1, contentId);
    insertPublication.bindValue(":channel_id"_L1, channelId);
    insertPublication.bindValue(":scheduled_at"_L1, publicationScheduledAt.toString(Qt::ISODate));
    QVERIFY(insertPublication.exec());

    const auto entries = dashboardRepository.calendarEntries(false, false);
    QVERIFY(std::ranges::any_of(entries, [&](const auto &entry) {
        return entry.contentId == contentId
            && entry.sourceType == "content"
            && entry.scheduledAt.toUTC().toSecsSinceEpoch() == contentScheduledAt.toUTC().toSecsSinceEpoch();
    }));
    QVERIFY(std::ranges::any_of(entries, [&](const auto &entry) {
        return entry.contentId == contentId
            && entry.sourceType == "publication"
            && entry.scheduledAt.toUTC().toSecsSinceEpoch() == publicationScheduledAt.toUTC().toSecsSinceEpoch();
    }));
}

void DatabaseTests::calendarFiltersArchivedAndPublishedAndMarksOverdue()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-calendar-filters"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};
    auto dashboardRepository = SmTool::Data::DashboardRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("video"));
    const auto channelId = lookups.lookupIdByKey(QStringLiteral("channel"), QStringLiteral("youtube"));

    const auto overdueContentId = contentRepository.create({
        .title = QStringLiteral("Overdue content"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("ready"),
        .priority = 10,
        .scheduledAt = QDateTime::currentDateTimeUtc().addDays(-1),
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!overdueContentId.isEmpty(), qPrintable(errorMessage));

    const auto archivedContentId = contentRepository.create({
        .title = QStringLiteral("Archived content"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("archived"),
        .priority = 10,
        .scheduledAt = QDateTime::currentDateTimeUtc().addDays(2),
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!archivedContentId.isEmpty(), qPrintable(errorMessage));

    const auto publishedContentId = contentRepository.create({
        .title = QStringLiteral("Published content"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("published"),
        .priority = 10,
        .scheduledAt = QDateTime::currentDateTimeUtc().addDays(1),
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!publishedContentId.isEmpty(), qPrintable(errorMessage));

    const auto plannedRepublishId = contentRepository.create({
        .title = QStringLiteral("Republish content"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("published"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!plannedRepublishId.isEmpty(), qPrintable(errorMessage));

    QSqlQuery insertPublication{database.connection()};
    insertPublication.prepare(QStringLiteral(
        "INSERT INTO publication "
        "(id, content_id, channel_id, status, scheduled_at, published_at, url, created_at, updated_at) "
        "VALUES (:id, :content_id, :channel_id, :status, :scheduled_at, NULL, '', datetime('now'), datetime('now'))"));

    insertPublication.bindValue(":id"_L1, QStringLiteral("pub-planned"));
    insertPublication.bindValue(":content_id"_L1, plannedRepublishId);
    insertPublication.bindValue(":channel_id"_L1, channelId);
    insertPublication.bindValue(":status"_L1, QStringLiteral("planned"));
    insertPublication.bindValue(":scheduled_at"_L1, QDateTime::currentDateTimeUtc().addDays(4).toString(Qt::ISODate));
    QVERIFY(insertPublication.exec());

    insertPublication.bindValue(":id"_L1, QStringLiteral("pub-published"));
    insertPublication.bindValue(":content_id"_L1, plannedRepublishId);
    insertPublication.bindValue(":channel_id"_L1, channelId);
    insertPublication.bindValue(":status"_L1, QStringLiteral("published"));
    insertPublication.bindValue(":scheduled_at"_L1, QDateTime::currentDateTimeUtc().addDays(5).toString(Qt::ISODate));
    QVERIFY(insertPublication.exec());

    const auto filteredEntries = dashboardRepository.calendarEntries(false, false);
    QVERIFY(std::ranges::any_of(filteredEntries, [&](const auto &entry) {
        return entry.contentId == overdueContentId && entry.isOverdue;
    }));
    QVERIFY(!std::ranges::any_of(filteredEntries, [&](const auto &entry) {
        return entry.contentId == archivedContentId;
    }));
    QVERIFY(!std::ranges::any_of(filteredEntries, [&](const auto &entry) {
        return entry.contentId == publishedContentId && entry.sourceType == "content";
    }));
    QVERIFY(std::ranges::any_of(filteredEntries, [&](const auto &entry) {
        return entry.id == "pub-planned"_L1 && entry.sourceType == "publication";
    }));
    QVERIFY(!std::ranges::any_of(filteredEntries, [&](const auto &entry) {
        return entry.id == "pub-published"_L1;
    }));

    const auto allEntries = dashboardRepository.calendarEntries(true, true);
    QVERIFY(std::ranges::any_of(allEntries, [&](const auto &entry) {
        return entry.contentId == archivedContentId;
    }));
    QVERIFY(std::ranges::any_of(allEntries, [&](const auto &entry) {
        return entry.contentId == publishedContentId && entry.sourceType == "content";
    }));
    QVERIFY(std::ranges::any_of(allEntries, [&](const auto &entry) {
        return entry.id == "pub-published"_L1;
    }));
}

void DatabaseTests::reopensDatabaseAtNewPath()
{
    const auto originalPath = createTempDatabasePath();
    const auto movedPath = createTempDatabasePath();

    SmTool::Data::Database database({
        .databaseFilePath = originalPath,
        .connectionName = QStringLiteral("test-reopen"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    QSqlQuery insertQuery{database.connection()};
    QVERIFY(insertQuery.exec(QStringLiteral("INSERT INTO pillar (id, key, display_name, description, sort_order, is_active) "
                                           "VALUES ('custom-pillar', 'custom', 'Custom', '', 99, 1)")));

    QVERIFY2(database.reopenAtPath(movedPath, &errorMessage), qPrintable(errorMessage));

    QVERIFY(QFileInfo::exists(movedPath));
    QVERIFY(!QFileInfo::exists(originalPath));
    QCOMPARE(QFileInfo{database.databasePath()}.absoluteFilePath(), QFileInfo{movedPath}.absoluteFilePath());

    QSqlQuery query{database.connection()};
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM pillar WHERE key = 'custom'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void DatabaseTests::createsIdeaFromPlainText()
{
    SmTool::App::AppController controller({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-controller-plain"),
    });
    QString errorMessage;
    QVERIFY2(controller.initialize(&errorMessage), qPrintable(errorMessage));

    QVERIFY(controller.createIdeaFromText(QStringLiteral("Build a local-first workflow tool for content teams. It should stay simple.")));
    QCOMPARE(controller.inboxModel()->rowCount(), 1);
    QCOMPARE(controller.inboxModel()->data(controller.inboxModel()->index(0, 0),
                                           SmTool::Models::ContentListModel::TitleRole).toString(),
             QStringLiteral("Build a local-first workflow tool"));
}

void DatabaseTests::createsIdeaFromMarkdown()
{
    SmTool::App::AppController controller({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-controller-markdown"),
    });
    QString errorMessage;
    QVERIFY2(controller.initialize(&errorMessage), qPrintable(errorMessage));

    const auto markdown = QStringLiteral("# Burst generation notes\n\n- Keep templates reusable\n- Preserve idempotency\n");
    QVERIFY(controller.createIdeaFromText(markdown));
    QCOMPARE(controller.inboxModel()->rowCount(), 1);
    QCOMPARE(controller.inboxModel()->data(controller.inboxModel()->index(0, 0),
                                           SmTool::Models::ContentListModel::TitleRole).toString(),
             QStringLiteral("Burst generation notes"));
}

void DatabaseTests::importsIdeaFromPlainTextFile()
{
    SmTool::App::AppController controller({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-import-plain-file"),
    });
    QString errorMessage;
    QVERIFY2(controller.initialize(&errorMessage), qPrintable(errorMessage));

    const auto filePath = writeTempTextFile(QStringLiteral(".txt"),
                                            QStringLiteral("Plain text import for inbox capture. Keep it simple."));
    QVERIFY(controller.importIdeasFromFile(filePath));
    QCOMPARE(controller.inboxModel()->rowCount(), 1);
    QCOMPARE(controller.inboxModel()->data(controller.inboxModel()->index(0, 0),
                                           SmTool::Models::ContentListModel::TitleRole).toString(),
             QStringLiteral("Plain text import for inbox"));
}

void DatabaseTests::importsIdeasFromMarkdownFileSections()
{
    SmTool::App::AppController controller({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-import-markdown-file"),
    });
    QString errorMessage;
    QVERIFY2(controller.initialize(&errorMessage), qPrintable(errorMessage));

    const auto markdown = QStringLiteral(
        "# First idea\n\nAlpha body.\n\n---\n\n# Second idea\n\nBeta body.\n");
    const auto filePath = writeTempTextFile(QStringLiteral(".md"), markdown);
    QVERIFY(controller.importIdeasFromFile(filePath));
    QCOMPARE(controller.inboxModel()->rowCount(), 2);
    QStringList titles;
    for (int row = 0; row < controller.inboxModel()->rowCount(); ++row) {
        titles.append(controller.inboxModel()->data(controller.inboxModel()->index(row, 0),
                                                    SmTool::Models::ContentListModel::TitleRole).toString());
    }
    QVERIFY(titles.contains(QStringLiteral("First idea")));
    QVERIFY(titles.contains(QStringLiteral("Second idea")));
}

void DatabaseTests::allowsArbitraryStatusTransition()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-any-status"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Transition item"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    QVERIFY2(contentRepository.updateStatus(contentId, QStringLiteral("published"), &errorMessage), qPrintable(errorMessage));
    QCOMPARE(contentRepository.getById(contentId).status, QStringLiteral("published"));
}

void DatabaseTests::preventsDeletingSystemOrReferencedStatuses()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-status-delete-guards"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    QSqlQuery deleteInbox{database.connection()};
    deleteInbox.prepare(QStringLiteral("DELETE FROM content_status WHERE id = 'inbox'"));
    QVERIFY(!deleteInbox.exec());

    QSqlQuery deleteArchived{database.connection()};
    deleteArchived.prepare(QStringLiteral("DELETE FROM content_status WHERE id = 'archived'"));
    QVERIFY(!deleteArchived.exec());

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Referenced status item"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("ready"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    QSqlQuery deleteReady{database.connection()};
    deleteReady.prepare(QStringLiteral("DELETE FROM content_status WHERE id = 'ready'"));
    QVERIFY(!deleteReady.exec());
}

void DatabaseTests::deletesContentTree()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-delete-content"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("blog_post"));
    const auto channelId = lookups.lookupIdByKey(QStringLiteral("channel"), QStringLiteral("youtube"));

    const auto sourceId = contentRepository.create({
        .title = QStringLiteral("Delete source"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("ready"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!sourceId.isEmpty(), qPrintable(errorMessage));

    const auto childId = contentRepository.create({
        .parentId = sourceId,
        .title = QStringLiteral("Delete child"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("shaping"),
        .priority = 5,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!childId.isEmpty(), qPrintable(errorMessage));

    QSqlQuery noteQuery{database.connection()};
    QVERIFY(noteQuery.exec(QStringLiteral(
        "INSERT INTO note (id, content_id, body, created_at, updated_at) "
        "VALUES ('note-1', '%1', 'n', datetime('now'), datetime('now'))").arg(sourceId)));

    QSqlQuery publicationQuery{database.connection()};
    publicationQuery.prepare(QStringLiteral(
        "INSERT INTO publication "
        "(id, content_id, channel_id, status, scheduled_at, published_at, url, created_at, updated_at) "
        "VALUES ('pub-1', :content_id, :channel_id, 'planned', NULL, NULL, '', datetime('now'), datetime('now'))"));
    publicationQuery.bindValue(":content_id"_L1, childId);
    publicationQuery.bindValue(":channel_id"_L1, channelId);
    QVERIFY(publicationQuery.exec());

    QVERIFY2(contentRepository.remove(sourceId, &errorMessage), qPrintable(errorMessage));
    QVERIFY(contentRepository.getById(sourceId).id.isEmpty());
    QVERIFY(contentRepository.getById(childId).id.isEmpty());

    QSqlQuery countQuery{database.connection()};
    QVERIFY(countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM publication WHERE id = 'pub-1'")));
    QVERIFY(countQuery.next());
    QCOMPARE(countQuery.value(0).toInt(), 0);
    QVERIFY(countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM note WHERE id = 'note-1'")));
    QVERIFY(countQuery.next());
    QCOMPARE(countQuery.value(0).toInt(), 0);
}

void DatabaseTests::updatesContentFields()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-update-content"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto otherPillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("product"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    const auto channelId = lookups.lookupIdByKey(QStringLiteral("channel"), QStringLiteral("linkedin"));
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Original"),
        .description = QStringLiteral("Before"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    auto item = contentRepository.getById(contentId);
    item.title = QStringLiteral("Updated");
    item.description = QStringLiteral("After");
    item.tags = QStringLiteral("#Second; first\tinvalid/tag second");
    item.pillarId = otherPillarId;
    item.suggestedChannelId = channelId;
    item.priority = 77;
    item.status = QStringLiteral("reviewing");
    item.scheduledAt = QDateTime::fromString(QStringLiteral("2026-06-15T00:00:00"), Qt::ISODate);
    QVERIFY2(contentRepository.update(item, &errorMessage), qPrintable(errorMessage));

    const auto updated = contentRepository.getById(contentId);
    QCOMPARE(updated.title, QStringLiteral("Updated"));
    QCOMPARE(updated.description, QStringLiteral("After"));
    QCOMPARE(updated.tags, QStringLiteral("first second"));
    QCOMPARE(updated.pillarId, otherPillarId);
    QCOMPARE(updated.suggestedChannelId, channelId);
    QCOMPARE(updated.priority, 77);
    QCOMPARE(updated.status, QStringLiteral("reviewing"));
    QCOMPARE(updated.scheduledAt, item.scheduledAt);
}

void DatabaseTests::normalizesAndCachesTags()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-tags"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Tagged"),
        .description = QStringLiteral("Testing tags"),
        .tags = QStringLiteral("#Qt6, build-log  Local_First;qt6 bad/tag"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    const auto content = contentRepository.getById(contentId);
    QCOMPARE(content.tags, QStringLiteral("build-log local_first qt6"));

    const auto inboxItems = contentRepository.inboxItems();
    const auto found = std::ranges::find_if(inboxItems, [&](const auto &item) { return item.id == contentId; });
    QVERIFY(found != inboxItems.end());
    QCOMPARE(found->tags, QStringLiteral("build-log local_first qt6"));

    QSqlQuery query{database.connection()};
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM tag")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM content_tag WHERE content_id = '%1'").arg(contentId)));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    QVERIFY(query.exec(QStringLiteral("SELECT tags_cache FROM content WHERE id = '%1'").arg(contentId)));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("build-log local_first qt6"));
}

void DatabaseTests::sortsAllContentByDueDateThenTitle()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-all-content-sort"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));

    QVERIFY2(!contentRepository.create({
        .title = QStringLiteral("Zulu"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 1,
        .scheduledAt = QDateTime::fromString(QStringLiteral("2026-06-20T00:00:00"), Qt::ISODate),
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage).isEmpty(), qPrintable(errorMessage));

    QVERIFY2(!contentRepository.create({
        .title = QStringLiteral("Alpha"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 1,
        .scheduledAt = QDateTime::fromString(QStringLiteral("2026-06-20T00:00:00"), Qt::ISODate),
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage).isEmpty(), qPrintable(errorMessage));

    QVERIFY2(!contentRepository.create({
        .title = QStringLiteral("No Date"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 1,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage).isEmpty(), qPrintable(errorMessage));

    const auto items = contentRepository.allItems(false, SmTool::Data::ContentRepository::SortMode::DueDateAlphabetical);
    QVERIFY(items.size() >= 3);
    QCOMPARE(items.at(0).title, QStringLiteral("Alpha"));
    QCOMPARE(items.at(1).title, QStringLiteral("Zulu"));
    QCOMPARE(items.back().title, QStringLiteral("No Date"));
}

void DatabaseTests::searchesContentByTextAndTags()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-content-search"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));

    QVERIFY2(!contentRepository.create({
        .title = QStringLiteral("Search Alpha"),
        .description = QStringLiteral("Deep description match"),
        .tags = QStringLiteral("#focus #alpha"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 1,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage).isEmpty(), qPrintable(errorMessage));

    QVERIFY2(!contentRepository.create({
        .title = QStringLiteral("Other Title"),
        .description = QStringLiteral("Different body"),
        .tags = QStringLiteral("#beta"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("inbox"),
        .priority = 1,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage).isEmpty(), qPrintable(errorMessage));

    QCOMPARE(contentRepository.inboxItems(QStringLiteral("alpha")).size(), 1);
    QCOMPARE(contentRepository.inboxItems(QStringLiteral("#focus")).size(), 1);
    QCOMPARE(contentRepository.inboxItems(QStringLiteral("t:search")).size(), 1);
    QCOMPARE(contentRepository.inboxItems(QStringLiteral("d:deep")).size(), 1);
    QCOMPARE(contentRepository.inboxItems(QStringLiteral("t:deep")).size(), 0);
}

void DatabaseTests::searchesSeriesByNameAndDescription()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-series-search"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto seriesRepository = SmTool::Data::SeriesRepository{database.connection()};
    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));

    QVERIFY2(!seriesRepository.create(QStringLiteral("Alpha Series"),
                                      QStringLiteral("Backend notes"),
                                      pillarId,
                                      QStringLiteral("active"),
                                      &errorMessage).isEmpty(), qPrintable(errorMessage));
    QVERIFY2(!seriesRepository.create(QStringLiteral("Beta Series"),
                                      QStringLiteral("Frontend notes"),
                                      pillarId,
                                      QStringLiteral("active"),
                                      &errorMessage).isEmpty(), qPrintable(errorMessage));

    QCOMPARE(seriesRepository.list(false, QStringLiteral("t:alpha")).size(), 1);
    QCOMPARE(seriesRepository.list(false, QStringLiteral("d:frontend")).size(), 1);
    QCOMPARE(seriesRepository.list(false, QStringLiteral("#tagged")).size(), 0);
}

void DatabaseTests::contentModelBuildsDescriptionPreview()
{
    SmTool::Models::ContentListModel model;
    model.setDescriptionPreviewWordCap(5);
    model.setItems({
        SmTool::Domain::ContentSummary{
            .id = QStringLiteral("a"),
            .title = QStringLiteral("Card title"),
            .description = QStringLiteral("This is the first sentence of a longer description. Second sentence here."),
            .tags = QStringLiteral("awesome goodideas"),
        }
    });

    QCOMPARE(model.data(model.index(0, 0), SmTool::Models::ContentListModel::TitleRole).toString(),
             QStringLiteral("Card title"));
    QCOMPARE(model.data(model.index(0, 0), SmTool::Models::ContentListModel::DescriptionPreviewRole).toString(),
             QStringLiteral("This is the first sentence..."));
    QCOMPARE(model.data(model.index(0, 0), SmTool::Models::ContentListModel::DisplayTagsRole).toString(),
             QStringLiteral("#awesome #goodideas"));
}

void DatabaseTests::publicationCrudWorks()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-publication-crud"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};
    auto publicationRepository = SmTool::Data::PublicationRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    const auto channelId = lookups.lookupIdByKey(QStringLiteral("channel"), QStringLiteral("linkedin"));
    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Publication source"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("ready"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    const auto publicationId = publicationRepository.create({
        .contentId = contentId,
        .channelId = channelId,
        .status = QStringLiteral("planned"),
        .scheduledAt = QDateTime::fromString(QStringLiteral("2026-06-10T09:30:00Z"), Qt::ISODate),
    }, &errorMessage);
    QVERIFY2(!publicationId.isEmpty(), qPrintable(errorMessage));

    auto publication = publicationRepository.getById(publicationId);
    QCOMPARE(publication.status, QStringLiteral("planned"));
    QCOMPARE(publication.channelId, channelId);

    publication.status = QStringLiteral("published");
    publication.url = QStringLiteral("https://example.com/post");
    publication.publishedAt = QDateTime::fromString(QStringLiteral("2026-06-12T12:00:00Z"), Qt::ISODate);
    QVERIFY2(publicationRepository.update(publication, &errorMessage), qPrintable(errorMessage));

    const auto updated = publicationRepository.getById(publicationId);
    QCOMPARE(updated.status, QStringLiteral("published"));
    QCOMPARE(updated.url, QStringLiteral("https://example.com/post"));

    const auto publications = publicationRepository.listForContent(contentId);
    QCOMPARE(static_cast<int>(publications.size()), 1);

    QVERIFY2(publicationRepository.remove(publicationId, &errorMessage), qPrintable(errorMessage));
    QVERIFY(publicationRepository.getById(publicationId).id.isEmpty());
}

void DatabaseTests::persistsMediaForContentAndPublication()
{
    SmTool::Data::Database database({
        .databaseFilePath = createTempDatabasePath(),
        .connectionName = QStringLiteral("test-media"),
    });
    QString errorMessage;
    QVERIFY2(database.initialize(&errorMessage), qPrintable(errorMessage));

    auto lookups = SmTool::Data::LookupsRepository{database.connection()};
    auto contentRepository = SmTool::Data::ContentRepository{database.connection()};
    auto publicationRepository = SmTool::Data::PublicationRepository{database.connection()};
    auto mediaRepository = SmTool::Data::MediaRepository{database.connection()};

    const auto pillarId = lookups.lookupIdByKey(QStringLiteral("pillar"), QStringLiteral("tech"));
    const auto kindId = lookups.lookupIdByKey(QStringLiteral("content_kind"), QStringLiteral("idea"));
    const auto channelId = lookups.lookupIdByKey(QStringLiteral("channel"), QStringLiteral("linkedin"));

    const auto contentId = contentRepository.create({
        .title = QStringLiteral("Media source"),
        .kindId = kindId,
        .pillarId = pillarId,
        .status = QStringLiteral("drafting"),
        .priority = 10,
        .createdAt = QDateTime::currentDateTimeUtc(),
        .updatedAt = QDateTime::currentDateTimeUtc(),
    }, &errorMessage);
    QVERIFY2(!contentId.isEmpty(), qPrintable(errorMessage));

    QVERIFY2(mediaRepository.replaceForContent(contentId, {
        SmTool::Domain::MediaItem{
            .name = QStringLiteral("Reference"),
            .sourceType = QStringLiteral("url"),
            .location = QStringLiteral("https://example.com/ref"),
        },
        SmTool::Domain::MediaItem{
            .name = QStringLiteral("Screenshot"),
            .sourceType = QStringLiteral("managed_file"),
            .location = QStringLiteral("media/example.png"),
        },
    }, &errorMessage), qPrintable(errorMessage));

    const auto contentMedia = mediaRepository.listForContent(contentId);
    QCOMPARE(static_cast<int>(contentMedia.size()), 2);
    QCOMPARE(contentMedia.front().name, QStringLiteral("Reference"));

    const auto publicationId = publicationRepository.create({
        .contentId = contentId,
        .channelId = channelId,
        .status = QStringLiteral("planned"),
    }, &errorMessage);
    QVERIFY2(!publicationId.isEmpty(), qPrintable(errorMessage));

    QVERIFY2(mediaRepository.replaceForPublication(publicationId, {
        SmTool::Domain::MediaItem{
            .name = QStringLiteral("Published page"),
            .sourceType = QStringLiteral("url"),
            .location = QStringLiteral("https://example.com/published"),
        },
    }, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(static_cast<int>(mediaRepository.listForPublication(publicationId).size()), 1);

    QVERIFY2(publicationRepository.remove(publicationId, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(static_cast<int>(mediaRepository.listForPublication(publicationId).size()), 0);

    QVERIFY2(contentRepository.remove(contentId, &errorMessage), qPrintable(errorMessage));
    QCOMPARE(static_cast<int>(mediaRepository.listForContent(contentId).size()), 0);
}

QTEST_MAIN(DatabaseTests)

#include "test_database.moc"
