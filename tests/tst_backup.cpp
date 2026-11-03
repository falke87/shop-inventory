#include <QtTest>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Backup.h"
#include "Database.h"
#include "InventoryService.h"

class TestBackup : public QObject
{
    Q_OBJECT

private slots:
    void backupIsAWorkingDatabase()
    {
        QTemporaryDir dir;
        Database db(":memory:");
        QVERIFY(db.open());
        InventoryService(db).addProduct("111111", "Rice", 5);

        const QDate today(2026, 11, 3);
        QString error;
        QVERIFY2(Backup::runIfDue(db.connection(), dir.path(), today, 30, &error), qPrintable(error));

        // Open the backup like the real program would.
        const QString path = dir.filePath("inventory-2026-11-03.db");
        QVERIFY(QFile::exists(path));
        Database copy(path);
        QVERIFY(copy.open());
        QVERIFY(InventoryService(copy).findByBarcode("111111").has_value());
    }

    void onlyOneBackupPerDay()
    {
        QTemporaryDir dir;
        Database db(":memory:");
        QVERIFY(db.open());

        const QDate today(2026, 11, 3);
        QVERIFY(Backup::runIfDue(db.connection(), dir.path(), today, 30));
        QVERIFY(Backup::runIfDue(db.connection(), dir.path(), today, 30));   // second call: no-op
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }

    void restoreBringsBackOldDataAndKeepsSafetyCopy()
    {
        QTemporaryDir dir;
        const QString backups = dir.filePath("backups");
        Database db(dir.filePath("inventory.db"));
        QVERIFY(db.open());
        InventoryService inventory(db);

        // Monday: only rice. Backup is made.
        inventory.addProduct("111111", "Rice", 5);
        QVERIFY(Backup::runIfDue(db.connection(), backups, QDate(2026, 11, 2), 30));
        // Later: tea is added.
        inventory.addProduct("222222", "Tea", 5);

        QString error;
        QVERIFY2(db.restoreFrom(backups + "/inventory-2026-11-02.db", &error), qPrintable(error));

        // Same Database object and same service keep working with the restored data.
        QVERIFY(inventory.findByBarcode("111111").has_value());
        QVERIFY(!inventory.findByBarcode("222222").has_value());

        // The data from before the restore (with tea) is kept as a safety copy.
        const QStringList safety = QDir(dir.path()).entryList({"before-restore-*.db"}, QDir::Files);
        QCOMPARE(safety.size(), 1);
        Database old(dir.filePath(safety.first()));
        QVERIFY(old.open());
        QVERIFY(InventoryService(old).findByBarcode("222222").has_value());

        // The backup file itself is unchanged and still there.
        QVERIFY(QFile::exists(backups + "/inventory-2026-11-02.db"));
    }

    void restoreFromWrongFileChangesNothing()
    {
        QTemporaryDir dir;
        Database db(dir.filePath("inventory.db"));
        QVERIFY(db.open());
        InventoryService inventory(db);
        inventory.addProduct("111111", "Rice", 5);

        // Not a database at all.
        QFile garbage(dir.filePath("photo.db"));
        QVERIFY(garbage.open(QIODevice::WriteOnly));
        garbage.write("this is not a database");
        garbage.close();

        QString error;
        QVERIFY(!db.restoreFrom(garbage.fileName(), &error));
        QVERIFY(error.contains("not a backup"));
        QVERIFY(inventory.findByBarcode("111111").has_value());   // still working, still there
        QVERIFY(!QFile::exists(dir.filePath("restore-in-progress.db")));
    }

    void oldBackupsAreDeleted()
    {
        QTemporaryDir dir;
        Database db(":memory:");
        QVERIFY(db.open());

        // Pretend there are already 4 older backups.
        for (const char *day : {"2026-10-30", "2026-10-31", "2026-11-01", "2026-11-02"}) {
            QFile file(dir.filePath(QString("inventory-%1.db").arg(day)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        QVERIFY(Backup::runIfDue(db.connection(), dir.path(), QDate(2026, 11, 3), 3));

        const QStringList left = QDir(dir.path()).entryList(QDir::Files, QDir::Name);
        QCOMPARE(left, QStringList({"inventory-2026-11-01.db",
                                    "inventory-2026-11-02.db",
                                    "inventory-2026-11-03.db"}));
    }
};

QTEST_GUILESS_MAIN(TestBackup)
#include "tst_backup.moc"
