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

    bool initialize(QString *errorMessage = nullptr);
    [[nodiscard]] QSqlDatabase connection() const;
    [[nodiscard]] QString databasePath() const;

private:
    bool open(QString *errorMessage);
    bool enableForeignKeys(QString *errorMessage);
    bool ensureMigrationTable(QString *errorMessage);
    bool applyMigrations(QString *errorMessage);
    bool seedDefaults(QString *errorMessage);
    bool seedDemoData(QString *errorMessage);
    [[nodiscard]] QString resourceMigrationPath() const;
    [[nodiscard]] QString effectiveConnectionName() const;

    Options options_;
    QString connectionName_;
};

} // namespace SmTool::Data
