#pragma once

#include <QList>
#include <optional>

#include "Types.h"

class Database;

// Everything that reads or changes stock.
//
// Every method that changes data runs in ONE transaction:
// either all of its rows are written, or none of them.
// That is what makes "no Save button" safe.
class InventoryService
{
public:
    // The Database must live longer than this object.
    explicit InventoryService(const Database &db);

    // ---- Products ----
    std::optional<Product> findByBarcode(const QString &barcode) const;
    QList<Product> searchByName(const QString &text, int limit = 20) const;
    QList<ProductStock> allProducts() const;

    // Returns the new product id, or -1 on error.
    int addProduct(const QString &barcode, const QString &name, int minStock,
                   QString *error = nullptr);
    bool updateProduct(int productId, const QString &name, int minStock,
                       QString *error = nullptr);

    // ---- Stock ----
    int stock(int productId) const;

    // Batches that still have goods, in the order they will be taken out:
    // earliest expiry first, batches without expiry date last.
    QList<BatchStock> batches(int productId) const;

    // Stock in: creates a new batch. An invalid 'expiry' means no expiry date.
    bool receive(int productId, int quantity, const QDate &expiry,
                 QString *error = nullptr);

    // Stock out: takes from the batch that expires first ("first expired,
    // first out"), moving on to the next batch if needed.
    bool issue(int productId, int quantity, QString *error = nullptr);

    // ---- Undo and history ----
    // The newest stock in/out that has not been undone yet.
    std::optional<OperationInfo> lastUndoable() const;

    // Cancels lastUndoable() by writing the opposite movements.
    bool undoLast(QString *error = nullptr);

    QList<OperationInfo> recentOperations(int limit = 10) const;

private:
    QList<OperationInfo> operations(bool onlyUndoable, int limit) const;

    const Database &m_db;
};
