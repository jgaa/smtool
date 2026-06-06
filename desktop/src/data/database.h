#pragma once

#include <QSqlDatabase>
#include <QString>

namespace SmTool::Data {

class Database
{
public:
    struct Options {
        bool seedDemoData = false;
        QString databaseFilePath;
        QString connectionName;
    };

    Database();
    explicit Database(Options options);
    ~Database();

    static QString defaultDatabasePath();
    static bool moveDatabaseFile(const QString &sourcePath, const QString &targetPath, QString *errorMessage = nullptr);

    bool initialize(QString *errorMessage = nullptr);
    bool reopenAtPath(const QString &path, QString *errorMessage = nullptr);
    [[nodiscard]] QSqlDatabase connection() const;
    [[nodiscard]] QString databasePath() const;

private:
    void close();
    bool open(QString *errorMessage);
    bool enableForeignKeys(QString *errorMessage);
    bool ensureMigrationTable(QString *errorMessage);
    bool applyMigrations(QString *errorMessage);
    bool seedDefaults(QString *errorMessage);
    bool seedDemoData(QString *errorMessage);
    [[nodiscard]] QString resourceMigrationPath(int version) const;
    [[nodiscard]] QString effectiveConnectionName() const;

    Options options_;
    QString connectionName_;
};

} // namespace SmTool::Data
