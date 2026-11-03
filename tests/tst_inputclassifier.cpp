#include <QtTest>

#include "InputClassifier.h"

Q_DECLARE_METATYPE(Input::Kind)

// Data-driven test: classify_data() lists the cases as rows of a table,
// classify() runs once for every row.
class TestInputClassifier : public QObject
{
    Q_OBJECT

private slots:
    void classify_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<Input::Kind>("expected");

        QTest::newRow("empty")            << ""              << Input::Kind::Empty;
        QTest::newRow("only spaces")      << "   "           << Input::Kind::Empty;
        QTest::newRow("quantity 1")       << "1"             << Input::Kind::Quantity;
        QTest::newRow("quantity 24")      << "24"            << Input::Kind::Quantity;
        QTest::newRow("quantity 9999")    << "9999"          << Input::Kind::Quantity;
        QTest::newRow("valid EAN-13")     << "4006381333931" << Input::Kind::Barcode;
        QTest::newRow("EAN-13 + spaces")  << " 4006381333931 " << Input::Kind::Barcode;
        QTest::newRow("typo in EAN-13")   << "4006381333932" << Input::Kind::InvalidBarcode;
        QTest::newRow("valid EAN-8")      << "96385074"      << Input::Kind::Barcode;
        QTest::newRow("valid UPC-A")      << "036000291452"  << Input::Kind::Barcode;
        QTest::newRow("other length")     << "12345"         << Input::Kind::Barcode;
        QTest::newRow("name")             << "rice"          << Input::Kind::Text;
        QTest::newRow("letters + digits") << "ABC-123"       << Input::Kind::Text;
    }

    void classify()
    {
        QFETCH(QString, input);
        QFETCH(Input::Kind, expected);
        QCOMPARE(Input::classify(input), expected);
    }

    void checkDigit()
    {
        QVERIFY(Input::isValidGtin("5901234123457"));
        QVERIFY(!Input::isValidGtin("5901234123458"));
        QVERIFY(!Input::isValidGtin("12345"));          // unknown length
        QVERIFY(!Input::isValidGtin("40063813339A1"));  // not digits
    }
};

QTEST_GUILESS_MAIN(TestInputClassifier)
#include "tst_inputclassifier.moc"
