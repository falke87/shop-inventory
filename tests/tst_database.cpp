#include <QtTest>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Database.h"

// Tests run against a real SQLite database kept in memory (":memory:").
// It is fast, and it checks the real SQL instead of a fake.
class TestDatabase : public QObject
{
    Q_OBJECT

private:
    // Small helper: run one statement and report whether it succeeded.
    static bool exec(const Database &db, const QString &sql)
    {
        QSqlQuery query(db.connection());
        return query.exec(sql);
    }

private slots:
    void newDatabaseGetsLatestVersion()
    {
        Database db(":memory:");
        QString error;
        QVERIFY2(db.open(&error), qPrintable(error));
        QCOMPARE(db.schemaVersion(), Database::LatestVersion);
    }

    void allTablesExist()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        const QStringList tables = db.connection().tables();
        QVERIFY(tables.contains("products"));
        QVERIFY(tables.contains("batches"));
        QVERIFY(tables.contains("operations"));
        QVERIFY(tables.contains("movements"));
        QVERIFY(tables.contains("settings"));
    }

    void defaultSettingsAreInserted()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        QSqlQuery query(db.connection());
        QVERIFY(query.exec("SELECT value FROM settings WHERE key = 'expiry_warning_days'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString("14"));
    }

    void barcodeMustBeUnique()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        QVERIFY(exec(db, "INSERT INTO products (barcode, name) VALUES ('4001234567890', 'Rice')"));
        QVERIFY(!exec(db, "INSERT INTO products (barcode, name) VALUES ('4001234567890', 'Noodles')"));
    }

    void foreignKeysAreEnforced()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        // Product 999 does not exist, so this batch must be rejected.
        QVERIFY(!exec(db, "INSERT INTO batches (product_id, received_at) VALUES (999, '2026-11-03')"));
    }

    void zeroQuantityIsRejected()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        QVERIFY(exec(db, "INSERT INTO products (id, barcode, name) VALUES (1, '111', 'Tea')"));
        QVERIFY(exec(db, "INSERT INTO batches (id, product_id, received_at) VALUES (1, 1, '2026-11-03')"));
        QVERIFY(exec(db, "INSERT INTO operations (id, kind, created_at) VALUES (1, 'receive', '2026-11-03')"));
        QVERIFY(!exec(db, "INSERT INTO movements (operation_id, batch_id, quantity) VALUES (1, 1, 0)"));
    }

    void sameOperationCannotBeUndoneTwice()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        QVERIFY(exec(db, "INSERT INTO operations (id, kind, created_at) VALUES (1, 'receive', '2026-11-03')"));

        // First undo is fine, the second one must fail (reverses_id is UNIQUE).
        QVERIFY(exec(db, "INSERT INTO operations (kind, created_at, reverses_id) VALUES ('undo', '2026-11-03', 1)"));
        QVERIFY(!exec(db, "INSERT INTO operations (kind, created_at, reverses_id) VALUES ('undo', '2026-11-03', 1)"));
    }

    void onlyUndoRowsHaveReversesId()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        QVERIFY(exec(db, "INSERT INTO operations (id, kind, created_at) VALUES (1, 'receive', '2026-11-03')"));
        // An undo without target, and a non-undo with a target: both wrong.
        QVERIFY(!exec(db, "INSERT INTO operations (kind, created_at) VALUES ('undo', '2026-11-03')"));
        QVERIFY(!exec(db, "INSERT INTO operations (kind, created_at, reverses_id) VALUES ('issue', '2026-11-03', 1)"));
        // An unknown kind is refused too.
        QVERIFY(!exec(db, "INSERT INTO operations (kind, created_at) VALUES ('sell', '2026-11-03')"));
    }

    void settingsCanBeReadAndWritten()
    {
        Database db(":memory:");
        QVERIFY(db.open());

        QCOMPARE(db.setting("expiry_warning_days"), QString("14"));
        QCOMPARE(db.setting("does_not_exist", "fallback"), QString("fallback"));
        QVERIFY(db.setSetting("expiry_warning_days", "30"));
        QCOMPARE(db.setting("expiry_warning_days"), QString("30"));
    }

    void reopeningKeepsDataAndDoesNotMigrateAgain()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.db");

        {
            Database db(path);
            QVERIFY(db.open());
            QVERIFY(exec(db, "INSERT INTO products (barcode, name) VALUES ('111', 'Tea')"));
        }   // db is closed here

        Database db(path);
        QString error;
        QVERIFY2(db.open(&error), qPrintable(error));   // would fail if CREATE TABLE ran again
        QCOMPARE(db.schemaVersion(), Database::LatestVersion);

        QSqlQuery query(db.connection());
        QVERIFY(query.exec("SELECT COUNT(*) FROM products"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }

    void newerFileIsRefused()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("future.db");

        {
            Database db(path);
            QVERIFY(db.open());
            QVERIFY(exec(db, QString("PRAGMA user_version = %1").arg(Database::LatestVersion + 1)));
        }

        Database db(path);
        QString error;
        QVERIFY(!db.open(&error));
        QVERIFY(error.contains("newer version"));
    }
};

// Creates a QCoreApplication (needed by the SQL driver) without any GUI.
QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
