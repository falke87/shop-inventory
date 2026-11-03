#include <QtTest>

#include "Database.h"
#include "InventoryService.h"

class TestInventory : public QObject
{
    Q_OBJECT

private:
    // Each test gets a fresh, empty in-memory database.
    // The Database must be declared before the service that uses it,
    // so that it is destroyed after it.
    struct Fixture
    {
        Database db{":memory:"};
        InventoryService inventory{db};
        int rice = 0;

        Fixture()
        {
            QVERIFY(db.open());
            rice = inventory.addProduct("4006381333931", "Jasmine rice", 5);
        }
    };

private slots:
    // ---------------------------------------------------- products

    void addAndFindProduct()
    {
        Fixture f;
        QVERIFY(f.rice > 0);

        const auto product = f.inventory.findByBarcode("4006381333931");
        QVERIFY(product.has_value());
        QCOMPARE(product->name, QString("Jasmine rice"));
        QCOMPARE(product->minStock, 5);

        QVERIFY(!f.inventory.findByBarcode("0000000000000").has_value());
    }

    void duplicateBarcodeGivesClearError()
    {
        Fixture f;
        QString error;
        QCOMPARE(f.inventory.addProduct("4006381333931", "Something else", 5, &error), -1);
        QVERIFY(error.contains("already exists"));
    }

    void searchByNameIgnoresCaseAndMatchesLiterally()
    {
        Fixture f;
        f.inventory.addProduct("111111", "Soy sauce 50% less salt", 5);
        f.inventory.addProduct("222222", "Soy sauce dark", 5);

        QCOMPARE(f.inventory.searchByName("RICE").size(), 1);
        QCOMPARE(f.inventory.searchByName("soy").size(), 2);
        // '%' is a wildcard in SQL; here it must only match a real '%'.
        QCOMPARE(f.inventory.searchByName("50%").size(), 1);
        QCOMPARE(f.inventory.searchByName("noodles").size(), 0);
    }

    void updateProductChangesNameAndMinimum()
    {
        Fixture f;
        QVERIFY(f.inventory.updateProduct(f.rice, "Jasmine rice 5kg", 3));
        const auto product = f.inventory.findByBarcode("4006381333931");
        QCOMPARE(product->name, QString("Jasmine rice 5kg"));
        QCOMPARE(product->minStock, 3);
    }

    // ------------------------------------------------- stock in/out

    void receiveIncreasesStock()
    {
        Fixture f;
        QCOMPARE(f.inventory.stock(f.rice), 0);
        QVERIFY(f.inventory.receive(f.rice, 24, QDate(2027, 1, 10)));
        QVERIFY(f.inventory.receive(f.rice, 6, QDate()));
        QCOMPARE(f.inventory.stock(f.rice), 30);
    }

    void zeroOrNegativeQuantityIsRejected()
    {
        Fixture f;
        QVERIFY(!f.inventory.receive(f.rice, 0, QDate()));
        QVERIFY(!f.inventory.receive(f.rice, -5, QDate()));
        QVERIFY(!f.inventory.issue(f.rice, 0));
        QCOMPARE(f.inventory.stock(f.rice), 0);
    }

    void issueTakesEarliestExpiryFirst()
    {
        Fixture f;
        QVERIFY(f.inventory.receive(f.rice, 5, QDate(2027, 3, 1)));
        QVERIFY(f.inventory.receive(f.rice, 5, QDate()));             // no expiry date
        QVERIFY(f.inventory.receive(f.rice, 5, QDate(2027, 1, 1)));   // expires first

        // 7 items: the whole January batch (5) + 2 from the March batch.
        QVERIFY(f.inventory.issue(f.rice, 7));

        const QList<BatchStock> left = f.inventory.batches(f.rice);
        QCOMPARE(left.size(), 2);
        QCOMPARE(left.at(0).expiry, QDate(2027, 3, 1));
        QCOMPARE(left.at(0).quantity, 3);
        QVERIFY(!left.at(1).expiry.isValid());   // undated batch comes last
        QCOMPARE(left.at(1).quantity, 5);
    }

    void issueMoreThanInStockChangesNothing()
    {
        Fixture f;
        QVERIFY(f.inventory.receive(f.rice, 3, QDate()));

        QString error;
        QVERIFY(!f.inventory.issue(f.rice, 4, &error));
        QVERIFY(error.contains("Only 3"));
        QCOMPARE(f.inventory.stock(f.rice), 3);
        QCOMPARE(f.inventory.recentOperations().size(), 1);   // only the receive
    }

    void issueFromEmptyStockSaysOutOfStock()
    {
        Fixture f;
        QString error;
        QVERIFY(!f.inventory.issue(f.rice, 1, &error));
        QVERIFY(error.contains("out of stock"));
    }

    // ------------------------------------------------------- undo

    void nothingToUndoAtStart()
    {
        Fixture f;
        QVERIFY(!f.inventory.lastUndoable().has_value());
        QVERIFY(!f.inventory.undoLast());
    }

    void undoGoesBackStepByStep()
    {
        Fixture f;
        QVERIFY(f.inventory.receive(f.rice, 24, QDate()));
        QVERIFY(f.inventory.issue(f.rice, 1));
        QCOMPARE(f.inventory.stock(f.rice), 23);

        const auto last = f.inventory.lastUndoable();
        QVERIFY(last.has_value());
        QCOMPARE(last->kind, OperationKind::Issue);
        QCOMPARE(last->quantity, 1);

        QVERIFY(f.inventory.undoLast());   // undoes the issue
        QCOMPARE(f.inventory.stock(f.rice), 24);
        QVERIFY(f.inventory.undoLast());   // undoes the receive
        QCOMPARE(f.inventory.stock(f.rice), 0);
        QVERIFY(!f.inventory.undoLast());  // nothing left
    }

    void undoOfIssueOverTwoBatchesRestoresBoth()
    {
        Fixture f;
        QVERIFY(f.inventory.receive(f.rice, 5, QDate(2027, 1, 1)));
        QVERIFY(f.inventory.receive(f.rice, 5, QDate(2027, 3, 1)));
        QVERIFY(f.inventory.issue(f.rice, 7));   // touches both batches

        QVERIFY(f.inventory.undoLast());
        const QList<BatchStock> batches = f.inventory.batches(f.rice);
        QCOMPARE(batches.size(), 2);
        QCOMPARE(batches.at(0).quantity, 5);
        QCOMPARE(batches.at(1).quantity, 5);
    }

    void historyMarksUndoneOperations()
    {
        Fixture f;
        QVERIFY(f.inventory.receive(f.rice, 24, QDate()));
        QVERIFY(f.inventory.issue(f.rice, 2));
        QVERIFY(f.inventory.undoLast());

        const QList<OperationInfo> history = f.inventory.recentOperations();
        QCOMPARE(history.size(), 2);              // undo rows are not listed themselves
        QCOMPARE(history.at(0).kind, OperationKind::Issue);
        QVERIFY(history.at(0).undone);
        QCOMPARE(history.at(1).kind, OperationKind::Receive);
        QVERIFY(!history.at(1).undone);
        QCOMPARE(history.at(0).productName, QString("Jasmine rice"));
    }

    void allProductsIncludesEmptyOnes()
    {
        Fixture f;
        f.inventory.addProduct("333333", "Fish sauce", 2);
        QVERIFY(f.inventory.receive(f.rice, 10, QDate()));

        const QList<ProductStock> all = f.inventory.allProducts();
        QCOMPARE(all.size(), 2);
        QCOMPARE(all.at(0).product.name, QString("Fish sauce"));   // sorted by name
        QCOMPARE(all.at(0).stock, 0);
        QCOMPARE(all.at(1).stock, 10);
    }
};

QTEST_GUILESS_MAIN(TestInventory)
#include "tst_inventory.moc"
