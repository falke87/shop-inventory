#include "Database.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

// Every migration is a list of SQL statements.
// Migration at index 0 brings the file from version 0 to 1,
// index 1 (added in the future) from 1 to 2, and so on.
//
// Statements are kept separate because Qt's SQLite driver
// executes only ONE statement per QSqlQuery::exec() call.
//
// Rule: never edit a migration that has already shipped.
// To change the schema, append a new migration instead.
const QList<QStringList> &migrations()
{
    static const QList<QStringList> list = {
        // ---- Version 1: initial schema ----
        {
            // One row per product. Every product has exactly one barcode.
            R"(CREATE TABLE products (
                   id         INTEGER PRIMARY KEY,
                   barcode    TEXT    NOT NULL UNIQUE,
                   name       TEXT    NOT NULL,
                   min_stock  INTEGER NOT NULL DEFAULT 5 CHECK (min_stock >= 0)
               ))",

            // One row per delivery of a product. Each delivery can have
            // its own expiry date. Dates are stored as 'YYYY-MM-DD' text,
            // which sorts correctly as text. NULL = no expiry date.
            R"(CREATE TABLE batches (
                   id           INTEGER PRIMARY KEY,
                   product_id   INTEGER NOT NULL REFERENCES products(id),
                   expiry_date  TEXT,
                   received_at  TEXT    NOT NULL
               ))",

            // One row per user action: one "stock in", one "stock out",
            // or one "undo". A single action can touch several batches
            // (taking 7 items may empty one batch and start the next),
            // so the stock changes themselves live in 'movements'.
            //
            // An undo is a new operation whose reverses_id points to the
            // operation it cancels. UNIQUE: an operation can be undone only
            // once. The CHECK: only undo rows have a reverses_id.
            R"(CREATE TABLE operations (
                   id           INTEGER PRIMARY KEY,
                   kind         TEXT    NOT NULL CHECK (kind IN ('receive', 'issue', 'undo')),
                   created_at   TEXT    NOT NULL,
                   reverses_id  INTEGER UNIQUE REFERENCES operations(id),
                   CHECK ((kind = 'undo') = (reverses_id IS NOT NULL))
               ))",

            // The stock ledger. Rows are only ever added, never changed
            // or deleted. Current stock of a batch = SUM(quantity).
            //   quantity > 0 : goods came in
            //   quantity < 0 : goods went out
            R"(CREATE TABLE movements (
                   id            INTEGER PRIMARY KEY,
                   operation_id  INTEGER NOT NULL REFERENCES operations(id),
                   batch_id      INTEGER NOT NULL REFERENCES batches(id),
                   quantity      INTEGER NOT NULL CHECK (quantity <> 0)
               ))",

            // Simple key/value settings.
            R"(CREATE TABLE settings (
                   key    TEXT PRIMARY KEY,
                   value  TEXT NOT NULL
               ))",

            "INSERT INTO settings (key, value) VALUES ('expiry_warning_days', '14')",
            "INSERT INTO settings (key, value) VALUES ('backup_folder', '')",

            // Indexes for the lookups we do all the time.
            "CREATE INDEX idx_batches_product ON batches(product_id)",
            "CREATE INDEX idx_movements_batch ON movements(batch_id)",
            "CREATE INDEX idx_movements_operation ON movements(operation_id)",
        },
    };
    return list;
}

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

} // namespace

Database::Database(const QString &filePath)
    : m_filePath(filePath)
{
    // Qt identifies connections by name. A unique name per object lets
    // several Database objects exist at the same time (useful in tests).
    static int counter = 0;
    m_connectionName = QStringLiteral("inventory_%1").arg(++counter);
}

Database::~Database()
{
    // removeDatabase() must be called only after every QSqlDatabase copy
    // is gone. The inner block makes sure our local copy is destroyed first.
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::open(QString *error)
{
    // The first call creates the connection; after restoreFrom() closed it,
    // a later call simply opens the same connection again.
    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
                          ? QSqlDatabase::database(m_connectionName, false)
                          : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_filePath);

    if (!db.open()) {
        setError(error, QStringLiteral("Cannot open database: ") + db.lastError().text());
        return false;
    }

    // SQLite ignores REFERENCES (foreign keys) unless this is switched on,
    // and it must be switched on again for every new connection.
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setError(error, query.lastError().text());
        return false;
    }

    return migrate(error);
}

int Database::schemaVersion() const
{
    QSqlQuery query(connection());
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        return query.value(0).toInt();
    return -1;
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

QString Database::filePath() const
{
    return m_filePath;
}

bool Database::restoreFrom(const QString &backupPath, QString *error)
{
    const QString folder = QFileInfo(m_filePath).absolutePath();
    const QString tempPath = folder + QStringLiteral("/restore-in-progress.db");
    const QString safetyPath = folder + QStringLiteral("/before-restore-%1.db")
                                            .arg(QDateTime::currentDateTime()
                                                     .toString(QStringLiteral("yyyy-MM-dd-HHmmss")));

    // 1. Work on a copy, so the backup file itself is never changed.
    QFile::remove(tempPath);
    if (!QFile::copy(backupPath, tempPath)) {
        setError(error, QObject::tr("Cannot read the backup file."));
        return false;
    }
    // A file copied from a USB stick or CD may be read-only. Our copy must be writable.
    QFile::setPermissions(tempPath, QFile::ReadOwner | QFile::WriteOwner
                                        | QFile::ReadUser | QFile::WriteUser);

    // 2. Check that the copy really is a database of this program.
    //    open() also upgrades a backup made by an older version.
    //    The inner block closes the check connection before we go on.
    bool usable = false;
    {
        Database check(tempPath);
        usable = check.open();
    }
    if (!usable) {
        QFile::remove(tempPath);
        setError(error, QObject::tr("This file is not a backup of this program."));
        return false;
    }

    // 3. Close our own connection: on Windows an open file cannot be renamed.
    connection().close();

    // 4. Swap the files. The current data is kept as a safety copy.
    if (!QFile::rename(m_filePath, safetyPath)) {
        QFile::remove(tempPath);
        open();   // nothing changed: just continue with the current data
        setError(error, QObject::tr("Cannot replace the current data."));
        return false;
    }
    if (!QFile::rename(tempPath, m_filePath)) {
        QFile::rename(safetyPath, m_filePath);   // put the current data back
        open();
        setError(error, QObject::tr("Cannot replace the current data."));
        return false;
    }

    // 5. Continue with the restored data.
    return open(error);
}

QString Database::setting(const QString &key, const QString &defaultValue) const
{
    QSqlQuery query(connection());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return defaultValue;
}

bool Database::setSetting(const QString &key, const QString &value)
{
    QSqlQuery query(connection());
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

bool Database::migrate(QString *error)
{
    const int current = schemaVersion();

    if (current < 0) {
        setError(error, QStringLiteral("Cannot read schema version."));
        return false;
    }
    if (current > LatestVersion) {
        // The file was created by a newer version of the program.
        // Refuse to touch it rather than risk damaging it.
        setError(error, QStringLiteral("The database was created by a newer version "
                                       "of this program (version %1).").arg(current));
        return false;
    }

    QSqlDatabase db = connection();

    for (int version = current; version < LatestVersion; ++version) {
        // Each migration runs in its own transaction: either every
        // statement succeeds, or the file stays exactly as it was.
        if (!db.transaction()) {
            setError(error, db.lastError().text());
            return false;
        }

        QSqlQuery query(db);
        for (const QString &statement : migrations().at(version)) {
            if (!query.exec(statement)) {
                setError(error, QStringLiteral("Migration to version %1 failed: %2")
                                    .arg(version + 1)
                                    .arg(query.lastError().text()));
                db.rollback();
                return false;
            }
        }

        // PRAGMA does not accept bound parameters, so the number is
        // written into the string. It is our own integer, not user input.
        if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version + 1))) {
            setError(error, query.lastError().text());
            db.rollback();
            return false;
        }

        if (!db.commit()) {
            setError(error, db.lastError().text());
            db.rollback();
            return false;
        }
    }

    return true;
}
