#pragma once

#include <QSqlDatabase>
#include <QString>

// Owns the connection to the SQLite file and keeps its schema up to date.
//
// Usage:
//     Database db("C:/.../inventory.db");   // or ":memory:" in tests
//     QString error;
//     if (!db.open(&error)) { show error; }
//
// The class is deliberately not copyable: one Database object = one connection.
class Database
{
public:
    // The schema version this build of the program expects.
    // Increase it by one every time a new migration is added.
    static constexpr int LatestVersion = 1;

    explicit Database(const QString &filePath);
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Opens the file (creates it if missing) and upgrades the schema.
    // Returns false and fills 'error' if something goes wrong.
    bool open(QString *error = nullptr);

    // The schema version stored inside the file (PRAGMA user_version).
    int schemaVersion() const;

    // The underlying Qt connection, used by the service classes.
    QSqlDatabase connection() const;

    QString filePath() const;

    // Replaces ALL current data with the backup file at 'backupPath'.
    // The backup file itself is not changed, and the current data is first
    // renamed to "before-restore-<date-time>.db" next to the database, so a
    // wrong choice can still be undone by hand.
    // On failure the current data stays in use.
    bool restoreFrom(const QString &backupPath, QString *error = nullptr);

    // Simple key/value settings stored in the 'settings' table.
    QString setting(const QString &key, const QString &defaultValue = QString()) const;
    bool setSetting(const QString &key, const QString &value);

private:
    bool migrate(QString *error);

    QString m_filePath;
    QString m_connectionName;
};
