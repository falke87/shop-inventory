#include <QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include "Database.h"
#include "InventoryService.h"
#include "MainWindow.h"

// Drives the real window with simulated key presses and clicks,
// exactly like a barcode scanner and a user would.
class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    static void typeAndEnter(MainWindow &w, const QString &text)
    {
        QTest::keyClicks(w.input(), text);
        QTest::keyClick(w.input(), Qt::Key_Return);
    }

    static void click(MainWindow &w, const char *buttonName)
    {
        auto *button = w.findChild<QPushButton *>(buttonName);
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
    }

    static QString message(MainWindow &w)
    {
        return w.findChild<QLabel *>("message")->text();
    }

private slots:
    void stockInOutAndUndo()
    {
        Database db(":memory:");
        QVERIFY(db.open());
        InventoryService inventory(db);
        const int rice = inventory.addProduct("4006381333931", "Jasmine rice", 5);

        MainWindow w(db);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Stock in 24: type the quantity, then "scan".
        click(w, "stockInButton");
        typeAndEnter(w, "24");
        typeAndEnter(w, "4006381333931");
        QCOMPARE(inventory.stock(rice), 24);

        // The quantity is reset after the scan: next scan is 1 piece.
        typeAndEnter(w, "4006381333931");
        QCOMPARE(inventory.stock(rice), 25);

        // Stock out one.
        click(w, "stockOutButton");
        typeAndEnter(w, "4006381333931");
        QCOMPARE(inventory.stock(rice), 24);

        // Undo the stock out.
        click(w, "undoButton");
        QCOMPARE(inventory.stock(rice), 25);
    }

    void searchByNameThenClick()
    {
        Database db(":memory:");
        QVERIFY(db.open());
        InventoryService inventory(db);
        const int rice = inventory.addProduct("4006381333931", "Jasmine rice", 5);

        MainWindow w(db);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        click(w, "stockInButton");
        typeAndEnter(w, "jasmine");                 // barcode label torn: search by name
        QCOMPARE(inventory.stock(rice), 0);         // nothing happens until the user clicks

        auto *results = w.findChild<QListWidget *>("results");
        QVERIFY(results);
        QCOMPARE(results->count(), 1);
        QListWidgetItem *item = results->item(0);
        QTest::mouseClick(results->viewport(), Qt::LeftButton, {},
                          results->visualItemRect(item).center());
        QCOMPARE(inventory.stock(rice), 1);
    }

    void invalidBarcodeIsRejected()
    {
        Database db(":memory:");
        QVERIFY(db.open());
        MainWindow w(db);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        click(w, "stockOutButton");
        typeAndEnter(w, "4006381333932");   // last digit wrong
        QVERIFY(message(w).contains("not a valid barcode"));
    }

    void stockOutOfUnknownProductIsRefused()
    {
        Database db(":memory:");
        QVERIFY(db.open());
        MainWindow w(db);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        click(w, "stockOutButton");
        typeAndEnter(w, "4006381333931");
        QVERIFY(message(w).contains("Unknown barcode"));
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
