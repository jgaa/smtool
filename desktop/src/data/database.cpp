#include "data/database.h"

#include "app/loggingcontroller.h"
#include "domain/constants.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <array>
#include <ranges>

using namespace Qt::Literals::StringLiterals;

namespace SmTool::Data {
namespace {

struct BurstTemplateSeed {
    QLatin1StringView key;
    QLatin1StringView displayName;
    QLatin1StringView titleSuffix;
    QLatin1StringView kindKey;
    QLatin1StringView channelKey;
    QLatin1StringView outcomeKey;
};

struct ContentStatusSeed {
    QLatin1StringView id;
    QLatin1StringView info;
    int sortOrder;
    bool isSystem;
};

constexpr auto burstTemplates = std::array{
    BurstTemplateSeed{"linkedin_key_lesson"_L1, "LinkedIn Key Lesson"_L1, " - Key Lesson"_L1, "short_post"_L1, "linkedin"_L1, "authority"_L1},
    BurstTemplateSeed{"linkedin_opinion_angle"_L1, "LinkedIn Opinion Angle"_L1, " - Opinion Angle"_L1, "short_post"_L1, "linkedin"_L1, "discussion"_L1},
    BurstTemplateSeed{"mastodon_technical_note"_L1, "Mastodon Technical Note"_L1, " - Technical Note"_L1, "short_post"_L1, "mastodon"_L1, "authority"_L1},
    BurstTemplateSeed{"short_video_excerpt"_L1, "Short Video Excerpt"_L1, " - Video Excerpt"_L1, "clip"_L1, "youtube"_L1, "trust"_L1},
    BurstTemplateSeed{"newsletter_summary"_L1, "Newsletter Summary"_L1, " - Newsletter Summary"_L1, "newsletter"_L1, "newsletter"_L1, "trust"_L1},
};

constexpr auto contentStatuses = std::array{
    ContentStatusSeed{"inbox"_L1, R"(## Workflow States

### Inbox

New ideas that have not yet been evaluated.

Questions:

* Is this worth keeping?
* Does it belong in smtool?
* Is it actionable?

Typical contents:

* Voice notes
* Random thoughts
* Links
* Half-formed ideas

Exit criteria:

* The idea is worth keeping.
* The basic intent is understood.

Move to:

* Clarifying)"_L1, 0, true},
    ContentStatusSeed{"clarifying"_L1, R"(## Workflow States

### Clarifying

Determine what the content is actually about.

Questions:

* What is the core idea?
* Why does this matter?
* Who is it for?
* Which pillar does it belong to?

Typical activities:

* Rewrite title
* Add notes
* Assign pillar
* Assign series
* Define audience

Exit criteria:

* The idea can be explained in one or two sentences.
* Target audience is understood.

Move to:

* Shaping)"_L1, 1, false},
    ContentStatusSeed{"shaping"_L1, R"(## Workflow States

### Shaping

Decide the form and structure of the content.

Questions:

* Blog post?
* Video?
* LinkedIn post?
* Series entry?
* Long-form source content?

Typical activities:

* Create outline
* Define deliverables
* Define desired outcome
* Decide whether this becomes source content for a burst

Exit criteria:

* Structure exists.
* Format has been chosen.
* Scope is understood.

Move to:

* Drafting)"_L1, 2, false},
    ContentStatusSeed{"drafting"_L1, R"(## Workflow States

### Drafting

Create the actual content.

Questions:

* What needs to be written, recorded, or designed?

Typical activities:

* Write draft
* Record video
* Create illustrations
* Gather screenshots
* Research details

Exit criteria:

* Content exists in a complete first version.

Move to:

* Ready)"_L1, 3, false},
    ContentStatusSeed{"ready"_L1, R"(## Workflow States

### Ready

Content is complete and awaiting scheduling.

Questions:

* Is this good enough to publish?
* Does it need final review?

Typical activities:

* Proofreading
* Final edits
* Metadata
* Thumbnail creation
* Link checks

Exit criteria:

* No further content work is required.

Move to:

* Scheduled)"_L1, 4, false},
    ContentStatusSeed{"scheduled"_L1, R"(## Workflow States

### Scheduled

Content has a planned publication date.

Questions:

* When should this be published?
* Which channel receives it?

Typical activities:

* Assign publish dates
* Create publication records
* Coordinate with related content

Exit criteria:

* Publication has occurred.

Move to:

* Published)"_L1, 5, false},
    ContentStatusSeed{"published"_L1, R"(## Workflow States

### Published

Content has been released.

Questions:

* Was publication successful?
* Were all intended channels used?

Typical activities:

* Record publication URLs
* Verify links
* Verify channel coverage

Exit criteria:

* Initial publication work is complete.

Move to:

* Reviewing)"_L1, 6, false},
    ContentStatusSeed{"reviewing"_L1, R"(## Workflow States

### Reviewing

Evaluate results and lessons learned.

Questions:

* Did the content achieve its goal?
* Should derivatives be created?
* Should similar content be created again?

Typical activities:

* Review engagement
* Capture lessons learned
* Identify follow-up ideas
* Create new content from insights

Exit criteria:

* Review completed.
* No immediate follow-up required.

Move to:

* Archived)"_L1, 7, false},
    ContentStatusSeed{"archived"_L1, R"(## Workflow States

### Archived

Content lifecycle is complete.

Purpose:

Historical reference and reporting.

Typical contents:

* Finished content
* Completed reviews
* Retired ideas

Notes:

Archived items remain searchable and reportable.
No further workflow actions are expected.)"_L1, 8, true},
};


QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QStringList splitSqlStatements(const QString &sql)
{
    QStringList statements;
    QString current;
    bool inTrigger = false;

    const auto lines = sql.split(u'\n');
    for (const auto &line : lines) {
        const auto trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (!current.isEmpty()) {
            current.append(u'\n');
        }
        current.append(line);

        if (!inTrigger && trimmed.startsWith(QStringLiteral("CREATE TRIGGER"), Qt::CaseInsensitive)) {
            inTrigger = true;
        }

        if (inTrigger) {
            if (trimmed == "END;"_L1) {
                statements.append(current.trimmed());
                current.clear();
                inTrigger = false;
            }
            continue;
        }

        if (trimmed.endsWith(u';')) {
            statements.append(current.trimmed());
            current.clear();
        }
    }

    if (!current.trimmed().isEmpty()) {
        statements.append(current.trimmed());
    }

    return statements;
}

bool executeQuery(QSqlQuery &query, QString *errorMessage)
{
    if (query.exec()) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool insertLookup(QSqlDatabase db, const QString &tableName, QAnyStringView key, int sortOrder, QString *errorMessage)
{
    QSqlQuery query{db};
    query.prepare(QStringLiteral(
                      "INSERT INTO %1 (id, key, display_name, description, sort_order, is_active) "
                      "VALUES (:id, :key, :display_name, '', :sort_order, 1)")
                      .arg(tableName));
    query.bindValue(":id"_L1, QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.bindValue(":key"_L1, key.toString());
    query.bindValue(":display_name"_L1, Domain::titleFromKey(key));
    query.bindValue(":sort_order"_L1, sortOrder);
    return executeQuery(query, errorMessage);
}

bool insertContentStatus(QSqlDatabase db, const ContentStatusSeed &seed, QString *errorMessage)
{
    QSqlQuery query{db};
    query.prepare(QStringLiteral(
        "INSERT INTO content_status (id, info, sort_order, is_system) "
        "VALUES (:id, :info, :sort_order, :is_system)"));
    query.bindValue(":id"_L1, seed.id.toString());
    query.bindValue(":info"_L1, seed.info.toString());
    query.bindValue(":sort_order"_L1, seed.sortOrder);
    query.bindValue(":is_system"_L1, seed.isSystem ? 1 : 0);
    return executeQuery(query, errorMessage);
}

QString lookupIdByKey(QSqlDatabase db, QAnyStringView tableName, QAnyStringView key, QString *errorMessage)
{
    QSqlQuery query{db};
    query.prepare(QStringLiteral("SELECT id FROM %1 WHERE key = :key").arg(tableName));
    query.bindValue(":key"_L1, key.toString());
    if (!executeQuery(query, errorMessage)) {
        return {};
    }
    if (!query.next()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Lookup key not found in %1: %2").arg(tableName, key);
        }
        return {};
    }
    return query.value(0).toString();
}

bool tableHasRows(QSqlDatabase db, QAnyStringView tableName, QString *errorMessage)
{
    QSqlQuery query{db};
    query.prepare(QStringLiteral("SELECT EXISTS(SELECT 1 FROM %1 LIMIT 1)").arg(tableName));
    if (!executeQuery(query, errorMessage)) {
        return false;
    }
    return query.next() && query.value(0).toBool();
}

} // namespace

Database::Database()
{
}

Database::Database(Options options)
    : options_(std::move(options))
{
}

Database::~Database()
{
    close();
}

QString Database::defaultDatabasePath()
{
    auto baseLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseLocation.isEmpty()) {
        baseLocation = QDir::home().filePath(QStringLiteral(".local/share/SmTool"));
    }
    return QDir{baseLocation}.filePath(QStringLiteral("smtool.sqlite"));
}

bool Database::moveDatabaseFile(const QString &sourcePath, const QString &targetPath, QString *errorMessage)
{
    const QFileInfo sourceInfo{sourcePath};
    const QFileInfo targetInfo{targetPath};
    if (sourceInfo.absoluteFilePath() == targetInfo.absoluteFilePath()) {
        return true;
    }
    if (!sourceInfo.exists()) {
        return true;
    }
    if (targetInfo.exists()) {
        return true;
    }

    QDir{}.mkpath(targetInfo.absolutePath());

    if (QFile::rename(sourceInfo.absoluteFilePath(), targetInfo.absoluteFilePath())) {
        return true;
    }

    if (QFile::copy(sourceInfo.absoluteFilePath(), targetInfo.absoluteFilePath())) {
        QFile::remove(sourceInfo.absoluteFilePath());
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Unable to move database from %1 to %2.")
                            .arg(sourceInfo.absoluteFilePath(), targetInfo.absoluteFilePath());
    }
    return false;
}

bool Database::initialize(QString *errorMessage)
{
    close();
    return open(errorMessage)
        && enableForeignKeys(errorMessage)
        && ensureMigrationTable(errorMessage)
        && applyMigrations(errorMessage)
        && seedDefaults(errorMessage)
        && (!options_.seedDemoData || seedDemoData(errorMessage));
}

bool Database::reopenAtPath(const QString &path, QString *errorMessage)
{
    const auto targetPath = path.trimmed().isEmpty() ? defaultDatabasePath() : QDir::cleanPath(path.trimmed());
    const auto currentPath = databasePath();
    if (QFileInfo{currentPath}.absoluteFilePath() == QFileInfo{targetPath}.absoluteFilePath()) {
        return true;
    }

    close();
    if (!moveDatabaseFile(currentPath, targetPath, errorMessage)) {
        options_.databaseFilePath = currentPath;
        initialize(errorMessage);
        return false;
    }

    options_.databaseFilePath = targetPath;
    if (initialize(errorMessage)) {
        LOG_INFO << "Database reopened at '" << targetPath.toStdString() << "'";
        return true;
    }

    return false;
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(connectionName_);
}

QString Database::databasePath() const
{
    if (!options_.databaseFilePath.isEmpty()) {
        return options_.databaseFilePath;
    }

    return defaultDatabasePath();
}

void Database::close()
{
    if (connectionName_.isEmpty()) {
        return;
    }

    {
        auto db = QSqlDatabase::database(connectionName_, false);
        if (db.isValid()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName_);
    connectionName_.clear();
}

bool Database::open(QString *errorMessage)
{
    connectionName_ = effectiveConnectionName();
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);

    const QFileInfo info{databasePath()};
    QDir{}.mkpath(info.absolutePath());
    db.setDatabaseName(info.absoluteFilePath());

    if (db.open()) {
        LOG_INFO << "Opened database '" << info.absoluteFilePath().toStdString() << "'";
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = db.lastError().text();
    }
    return false;
}

bool Database::enableForeignKeys(QString *errorMessage)
{
    QSqlQuery query{connection()};
    query.prepare(QStringLiteral("PRAGMA foreign_keys = ON"));
    return executeQuery(query, errorMessage);
}

bool Database::ensureMigrationTable(QString *errorMessage)
{
    QSqlQuery query{connection()};
    query.prepare(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INTEGER PRIMARY KEY,"
        "applied_at DATETIME NOT NULL)"));
    return executeQuery(query, errorMessage);
}

bool Database::applyMigrations(QString *errorMessage)
{
    QSqlQuery currentVersionQuery{connection()};
    currentVersionQuery.prepare(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations"));
    if (!executeQuery(currentVersionQuery, errorMessage) || !currentVersionQuery.next()) {
        return false;
    }

    const auto currentVersion = currentVersionQuery.value(0).toInt();
    constexpr int latestVersion = 3;
    if (currentVersion >= latestVersion) {
        return true;
    }

    for (int version = currentVersion + 1; version <= latestVersion; ++version) {
        QFile migrationFile{resourceMigrationPath(version)};
        if (!migrationFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Unable to open migration file: %1").arg(resourceMigrationPath(version));
            }
            return false;
        }

        const auto migrationSql = QString::fromUtf8(migrationFile.readAll());
        const auto statements = splitSqlStatements(migrationSql);
        auto db = connection();
        if (!db.transaction()) {
            if (errorMessage != nullptr) {
                *errorMessage = db.lastError().text();
            }
            return false;
        }

        for (const auto &rawStatement : statements) {
            const auto statement = rawStatement.trimmed();
            if (statement.isEmpty()) {
                continue;
            }

            QSqlQuery migrationQuery{db};
            if (!migrationQuery.exec(statement)) {
                db.rollback();
                if (errorMessage != nullptr) {
                    *errorMessage = migrationQuery.lastError().text();
                }
                return false;
            }
        }

        QSqlQuery insertVersionQuery{db};
        insertVersionQuery.prepare(QStringLiteral(
            "INSERT INTO schema_migrations(version, applied_at) VALUES (:version, :applied_at)"));
        insertVersionQuery.bindValue(":version"_L1, version);
        insertVersionQuery.bindValue(":applied_at"_L1, nowIso());
        if (!executeQuery(insertVersionQuery, errorMessage)) {
            db.rollback();
            return false;
        }

        if (!db.commit()) {
            if (errorMessage != nullptr) {
                *errorMessage = db.lastError().text();
            }
            return false;
        }
    }

    return true;
}

bool Database::seedDefaults(QString *errorMessage)
{
    auto db = connection();
    if (!db.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    auto rollbackOnFailure = [&db]() {
        db.rollback();
        return false;
    };

    if (!tableHasRows(db, "content_status"_L1, errorMessage)) {
        for (const auto &seed : contentStatuses) {
            if (!insertContentStatus(db, seed, errorMessage)) {
                return rollbackOnFailure();
            }
        }
    }

    if (!tableHasRows(db, "pillar"_L1, errorMessage)) {
        for (int index = 0; index < static_cast<int>(Domain::seededPillars.size()); ++index) {
            if (!insertLookup(db, QStringLiteral("pillar"), Domain::seededPillars.at(index), index, errorMessage)) {
                return rollbackOnFailure();
            }
        }
    }

    if (!tableHasRows(db, "content_kind"_L1, errorMessage)) {
        for (int index = 0; index < static_cast<int>(Domain::seededContentKinds.size()); ++index) {
            if (!insertLookup(db, QStringLiteral("content_kind"), Domain::seededContentKinds.at(index), index, errorMessage)) {
                return rollbackOnFailure();
            }
        }
    }

    if (!tableHasRows(db, "outcome"_L1, errorMessage)) {
        for (int index = 0; index < static_cast<int>(Domain::seededOutcomes.size()); ++index) {
            if (!insertLookup(db, QStringLiteral("outcome"), Domain::seededOutcomes.at(index), index, errorMessage)) {
                return rollbackOnFailure();
            }
        }
    }

    if (!tableHasRows(db, "channel"_L1, errorMessage)) {
        for (int index = 0; index < static_cast<int>(Domain::seededChannels.size()); ++index) {
            if (!insertLookup(db, QStringLiteral("channel"), Domain::seededChannels.at(index), index, errorMessage)) {
                return rollbackOnFailure();
            }
        }
    }

    if (!tableHasRows(db, "burst_template"_L1, errorMessage)) {
        for (const auto &seed : burstTemplates) {
            QSqlQuery insertQuery{db};
            insertQuery.prepare(QStringLiteral(
                "INSERT INTO burst_template "
                "(key, display_name, title_suffix, kind_id, suggested_channel_id, outcome_id, is_active) "
                "VALUES (:key, :display_name, :title_suffix, :kind_id, :suggested_channel_id, :outcome_id, 1)"));
            insertQuery.bindValue(":key"_L1, seed.key.toString());
            insertQuery.bindValue(":display_name"_L1, seed.displayName.toString());
            insertQuery.bindValue(":title_suffix"_L1, seed.titleSuffix.toString());
            insertQuery.bindValue(":kind_id"_L1, lookupIdByKey(db, "content_kind"_L1, seed.kindKey, errorMessage));
            insertQuery.bindValue(":suggested_channel_id"_L1, lookupIdByKey(db, "channel"_L1, seed.channelKey, errorMessage));
            insertQuery.bindValue(":outcome_id"_L1, lookupIdByKey(db, "outcome"_L1, seed.outcomeKey, errorMessage));
            if (!executeQuery(insertQuery, errorMessage)) {
                return rollbackOnFailure();
            }
        }
    }

    return db.commit();
}

bool Database::seedDemoData(QString *errorMessage)
{
    auto db = connection();
    if (tableHasRows(db, "series"_L1, errorMessage) || tableHasRows(db, "content"_L1, errorMessage)) {
        return true;
    }

    const auto now = nowIso();
    const auto pillarId = lookupIdByKey(db, "pillar"_L1, "tech"_L1, errorMessage);
    const auto videoKindId = lookupIdByKey(db, "content_kind"_L1, "video"_L1, errorMessage);
    const auto blogKindId = lookupIdByKey(db, "content_kind"_L1, "blog_post"_L1, errorMessage);
    const auto ideaKindId = lookupIdByKey(db, "content_kind"_L1, "idea"_L1, errorMessage);
    const auto authorityOutcomeId = lookupIdByKey(db, "outcome"_L1, "authority"_L1, errorMessage);
    const auto linkedinChannelId = lookupIdByKey(db, "channel"_L1, "linkedin"_L1, errorMessage);
    if (pillarId.isEmpty() || videoKindId.isEmpty() || blogKindId.isEmpty() || ideaKindId.isEmpty() || authorityOutcomeId.isEmpty()) {
        return false;
    }

    if (!db.transaction()) {
        if (errorMessage != nullptr) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    const auto seriesId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery seriesQuery{db};
    seriesQuery.prepare(QStringLiteral(
        "INSERT INTO series (id, name, description, pillar_id, status, created_at, updated_at) "
        "VALUES (:id, :name, :description, :pillar_id, 'active', :created_at, :updated_at)"));
    seriesQuery.bindValue(":id"_L1, seriesId);
    seriesQuery.bindValue(":name"_L1, QStringLiteral("Building SmTool"));
    seriesQuery.bindValue(":description"_L1, QStringLiteral("Public build notes and derivative ideas."));
    seriesQuery.bindValue(":pillar_id"_L1, pillarId);
    seriesQuery.bindValue(":created_at"_L1, now);
    seriesQuery.bindValue(":updated_at"_L1, now);
    if (!executeQuery(seriesQuery, errorMessage)) {
        db.rollback();
        return false;
    }

    const auto insertContent = [&](const QString &title,
                                   const QString &description,
                                   const QString &status,
                                   const QString &kindId,
                                   int priority,
                                   const QDateTime &scheduledAt,
                                   const QString &suggestedChannelId = {}) {
        QSqlQuery query{db};
        query.prepare(QStringLiteral(
            "INSERT INTO content "
            "(id, parent_id, series_id, burst_template_key, title, description, kind_id, pillar_id, outcome_id, suggested_channel_id, status, priority, scheduled_at, published_at, published_url, created_at, updated_at) "
            "VALUES (:id, NULL, :series_id, NULL, :title, :description, :kind_id, :pillar_id, :outcome_id, :suggested_channel_id, :status, :priority, :scheduled_at, NULL, '', :created_at, :updated_at)"));
        query.bindValue(":id"_L1, QUuid::createUuid().toString(QUuid::WithoutBraces));
        query.bindValue(":series_id"_L1, seriesId);
        query.bindValue(":title"_L1, title);
        query.bindValue(":description"_L1, description);
        query.bindValue(":kind_id"_L1, kindId);
        query.bindValue(":pillar_id"_L1, pillarId);
        query.bindValue(":outcome_id"_L1, authorityOutcomeId);
        query.bindValue(":suggested_channel_id"_L1, suggestedChannelId);
        query.bindValue(":status"_L1, status);
        query.bindValue(":priority"_L1, priority);
        query.bindValue(":scheduled_at"_L1, scheduledAt.isValid() ? scheduledAt.toString(Qt::ISODate) : QVariant{QMetaType::fromType<QString>()});
        query.bindValue(":created_at"_L1, now);
        query.bindValue(":updated_at"_L1, now);
        return executeQuery(query, errorMessage);
    };

    if (!insertContent(QStringLiteral("Ship the first SmTool POC"),
                       QStringLiteral("Focus on workflow and burst generation."),
                       QStringLiteral("drafting"),
                       blogKindId,
                       90,
                       QDateTime::currentDateTimeUtc().addDays(1))) {
        db.rollback();
        return false;
    }
    if (!insertContent(QStringLiteral("Record demo walkthrough"),
                       QStringLiteral("Short setup video once the board works."),
                       QStringLiteral("scheduled"),
                       videoKindId,
                       70,
                       QDateTime::currentDateTimeUtc().addDays(2),
                       linkedinChannelId)) {
        db.rollback();
        return false;
    }
    if (!insertContent(QStringLiteral("Inbox capture from coaching call"),
                       QStringLiteral(),
                       QStringLiteral("inbox"),
                       ideaKindId,
                       30,
                       {})) {
        db.rollback();
        return false;
    }

    return db.commit();
}

QString Database::resourceMigrationPath(int version) const
{
    switch (version) {
    case 1:
        return QStringLiteral(":/resources/migrations/001_initial.sql");
    case 2:
        return QStringLiteral(":/resources/migrations/002_goals.sql");
    case 3:
        return QStringLiteral(":/resources/migrations/003_series_positions.sql");
    default:
        return {};
    }
}

QString Database::effectiveConnectionName() const
{
    if (!options_.connectionName.isEmpty()) {
        return options_.connectionName;
    }

    return QStringLiteral("SmTool-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

} // namespace SmTool::Data
