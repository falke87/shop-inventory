#pragma once

#include <QDate>
#include <QWidget>

#include "AlertService.h"
#include "InventoryService.h"

class Database;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;

// The one and only window.
//
// Layout:
//   [ Stock in ] [ Stock out ] [ Look up ]                [Settings]
//   [ input field ...................................... ]
//   [ message .......................................... ]
//   | page: stock in/out  or  look-up table |  warnings  |
//   [ Undo ................................. ]
//
// How input works (the same for scanner and keyboard):
//   - type a small number (1-4 digits) + Enter  -> quantity for the next scan
//   - scan or type a barcode + Enter            -> act on that product
//   - type part of a name + Enter               -> list of matches to click
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Receive, Issue, Lookup };
    enum class MessageType { Info, Success, Error };

    explicit MainWindow(Database &db, QWidget *parent = nullptr);

    QLineEdit *input() const { return m_input; }

    // Reloads history, undo button and warnings from the database.
    void refresh();

    void showMessage(const QString &text, MessageType type);

private:
    void setMode(Mode mode);
    void handleInput();
    void actOnProduct(const Product &product);
    void handleUnknownBarcode(const QString &barcode);
    void showSearchResults(const QString &text);
    void undo();
    void openSettings();
    void restoreFromBackup();

    // "Next scan" panel
    void resetNextScan();
    void updateNextScanLabel();
    void chooseExpiryDate();

    // Look-up page
    void fillTable(const QString &filter);
    void selectProductInTable(int productId);
    void showSelectedProductDetails();
    void editSelectedProduct();
    std::optional<Product> selectedProduct() const;

    Database &m_db;
    InventoryService m_inventory;
    AlertService m_alerts;

    Mode m_mode = Mode::Receive;
    int m_nextQuantity = 1;
    QDate m_nextExpiry;          // invalid = no expiry date
    QString m_tableFilter;

    // Widgets (owned by Qt's parent/child system)
    QPushButton *m_receiveButton = nullptr;
    QPushButton *m_issueButton = nullptr;
    QPushButton *m_lookupButton = nullptr;
    QLineEdit *m_input = nullptr;
    QLabel *m_message = nullptr;
    QStackedWidget *m_pages = nullptr;

    QLabel *m_nextScanLabel = nullptr;
    QPushButton *m_expiryButton = nullptr;
    QLabel *m_resultsLabel = nullptr;
    QListWidget *m_results = nullptr;
    QListWidget *m_recent = nullptr;

    QTableWidget *m_table = nullptr;
    QLabel *m_details = nullptr;
    QPushButton *m_editButton = nullptr;

    QPushButton *m_undoButton = nullptr;
    QLabel *m_lowLabel = nullptr;
    QListWidget *m_lowList = nullptr;
    QLabel *m_expiryLabel = nullptr;
    QListWidget *m_expiryList = nullptr;
};
