#include <QtTest>

#include "AlertService.h"
#include "Database.h"
#include "InventoryService.h"

class TestAlerts : public QObject
{
    Q_OBJECT

private slots:
    void lowStockAtOrBelowMinimum()
    {
        Database db(":memory:");
        QVERIFY(db.open());
        InventoryService inventory(db);
        AlertService alerts(db);

        const int rice = inventory.addProduct("111111", "Rice", 5);
        const int tea = inventory.addProduct("222222", "Tea", 5);
        const int sauce = inventory.addProduct("333333", "Sauce", 5);
        inventory.receive(rice, 5, QDate());    // exactly the minimum -> warn
        inventory.receive(tea, 6, QDate());     // above -> no warning
        Q_UNUSED(sauce)                         // 0 in stock -> warn

        const QList<LowStockAlert> low = alerts.lowStock();
        QCOMPARE(low.size(), 2);
        QCOMPARE(low.at(0).productName, QString("Sauce"));   // emptiest first
        QCOMPARE(low.at(0).stock, 0);
        QCOMPARE(low.at(1).productName, QString("Rice"));
    }

    void expiringSoon()
    {
        Database db(":memory:");
        QVERIFY(db.open());
        InventoryService inventory(db);
        AlertService alerts(db);

        const QDate today(2026, 11, 3);   // fixed date: the test never depends on the real clock
        const int rice = inventory.addProduct("111111", "Rice", 0);
        const int tofu = inventory.addProduct("222222", "Tofu", 0);
        const int tea = inventory.addProduct("333333", "Tea", 0);
        const int milk = inventory.addProduct("444444", "Milk", 0);

        inventory.receive(rice, 3, today.addDays(30));   // too far away
        inventory.receive(tofu, 2, today.addDays(5));    // within 14 days
        inventory.receive(tea, 1, today.addDays(-2));    // already expired
        inventory.receive(milk, 1, today.addDays(1));    // within, but sold out:
        inventory.issue(milk, 1);                        //   nothing left -> no warning
        inventory.receive(rice, 4, QDate());             // no expiry date -> never warned

        const QList<ExpiryAlert> list = alerts.expiringSoon(today, 14);
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).productName, QString("Tea"));   // earliest date first
        QCOMPARE(list.at(0).daysLeft, -2);
        QCOMPARE(list.at(1).productName, QString("Tofu"));
        QCOMPARE(list.at(1).daysLeft, 5);
        QCOMPARE(list.at(1).quantity, 2);
    }
};

QTEST_GUILESS_MAIN(TestAlerts)
#include "tst_alerts.moc"
