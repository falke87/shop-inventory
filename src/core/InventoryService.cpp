#include "InventoryService.h"

#include "Database.h"

#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString nowText()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

// Reads a product from a query whose first columns are
// id, barcode, name, min_stock (in that order).
Product readProduct(const QSqlQuery &query)
{
    Product p;
    p.id = query.value(0).toInt();
    p.barcode = query.value(1).toString();
    p.name = query.value(2).toString();
    p.minStock = query.value(3).toInt();
    return p;
}

// LIKE treats % and _ as wildcards. Escape them so that what the user
// types is matched literally, e.g. "50%" finds "Soy sauce 50% less salt".
QString likePattern(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    escaped.replace(QLatin1String("%"), QLatin1String("\\%"));
    escaped.replace(QLatin1String("_"), QLatin1String("\\_"));
    return QLatin1Char('%') + escaped + QLatin1Char('%');
}

// Writes one row into 'operations'. Returns its id, or -1 on failure.
int insertOperation(QSqlQuery &query, const QString &kind, const QVariant &reversesId = QVariant())
{
    query.prepare(QStringLiteral(
        "INSERT INTO operations (kind, created_at, reverses_id) VALUES (?, ?, ?)"));
    query.addBindValue(kind);
    query.addBindValue(nowText());
    query.addBindValue(reversesId);   // an empty QVariant is stored as NULL
    if (!query.exec())
        return -1;
    return query.lastInsertId().toInt();
}

bool insertMovement(QSqlQuery &query, int operationId, int batchId, int quantity)
{
    query.prepare(QStringLiteral(
        "INSERT INTO movements (operation_id, batch_id, quantity) VALUES (?, ?, ?)"));
    query.addBindValue(operationId);
    query.addBindValue(batchId);
    query.addBindValue(quantity);
    return query.exec();
}

} // namespace

InventoryService::InventoryService(const Database &db)
    : m_db(db)
{
}

// ---------------------------------------------------------------- Products

std::optional<Product> InventoryService::findByBarcode(const QString &barcode) const
{
    QSqlQuery query(m_db.connection());
    query.prepare(QStringLiteral(
        "SELECT id, barcode, name, min_stock FROM products WHERE barcode = ?"));
    query.addBindValue(barcode);
    if (query.exec() && query.next())
        return readProduct(query);
    return std::nullopt;
}

QList<Product> InventoryService::searchByName(const QString &text, int limit) const
{
    QList<Product> result;
    QSqlQuery query(m_db.connection());
    // LIKE is case-insensitive for English letters in SQLite.
    query.prepare(QStringLiteral(
        "SELECT id, barcode, name, min_stock FROM products "
        "WHERE name LIKE ? ESCAPE '\\' "
        "ORDER BY name COLLATE NOCASE LIMIT ?"));
    query.addBindValue(likePattern(text.trimmed()));
    query.addBindValue(limit);
    if (query.exec()) {
        while (query.next())
            result.append(readProduct(query));
    }
    return result;
}

QList<ProductStock> InventoryService::allProducts() const
{
    QList<ProductStock> result;
    QSqlQuery query(m_db.connection());
    // LEFT JOIN so that products without any movement still appear (stock 0).
    const bool ok = query.exec(QStringLiteral(
        "SELECT p.id, p.barcode, p.name, p.min_stock, COALESCE(SUM(m.quantity), 0) "
        "FROM products p "
        "LEFT JOIN batches b ON b.product_id = p.id "
        "LEFT JOIN movements m ON m.batch_id = b.id "
        "GROUP BY p.id "
        "ORDER BY p.name COLLATE NOCASE"));
    if (ok) {
        while (query.next())
            result.append({readProduct(query), query.value(4).toInt()});
    }
    return result;
}

int InventoryService::addProduct(const QString &barcode, const QString &name, int minStock,
                                 QString *error)
{
    const QString cleanBarcode = barcode.trimmed();
    const QString cleanName = name.trimmed();

    if (cleanBarcode.isEmpty() || cleanName.isEmpty()) {
        setError(error, QObject::tr("Barcode and name must not be empty."));
        return -1;
    }
    // The UNIQUE constraint would reject it anyway; checking first gives
    // the user a clear message instead of a database error text.
    if (findByBarcode(cleanBarcode)) {
        setError(error, QObject::tr("A product with this barcode already exists."));
        return -1;
    }

    QSqlQuery query(m_db.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO products (barcode, name, min_stock) VALUES (?, ?, ?)"));
    query.addBindValue(cleanBarcode);
    query.addBindValue(cleanName);
    query.addBindValue(minStock);
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool InventoryService::updateProduct(int productId, const QString &name, int minStock,
                                     QString *error)
{
    if (name.trimmed().isEmpty()) {
        setError(error, QObject::tr("Name must not be empty."));
        return false;
    }

    QSqlQuery query(m_db.connection());
    query.prepare(QStringLiteral("UPDATE products SET name = ?, min_stock = ? WHERE id = ?"));
    query.addBindValue(name.trimmed());
    query.addBindValue(minStock);
    query.addBindValue(productId);
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return false;
    }
    return true;
}

// ------------------------------------------------------------------- Stock

int InventoryService::stock(int productId) const
{
    QSqlQuery query(m_db.connection());
    query.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(m.quantity), 0) "
        "FROM movements m JOIN batches b ON b.id = m.batch_id "
        "WHERE b.product_id = ?"));
    query.addBindValue(productId);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return 0;
}

QList<BatchStock> InventoryService::batches(int productId) const
{
    QList<BatchStock> result;
    QSqlQuery query(m_db.connection());
    // "expiry_date IS NULL" is 0 for dated batches and 1 for undated ones,
    // so sorting by it first puts batches without expiry date at the end.
    query.prepare(QStringLiteral(
        "SELECT b.id, b.expiry_date, SUM(m.quantity) AS qty "
        "FROM batches b JOIN movements m ON m.batch_id = b.id "
        "WHERE b.product_id = ? "
        "GROUP BY b.id "
        "HAVING qty > 0 "
        "ORDER BY b.expiry_date IS NULL, b.expiry_date, b.id"));
    query.addBindValue(productId);
    if (query.exec()) {
        while (query.next()) {
            BatchStock batch;
            batch.batchId = query.value(0).toInt();
            batch.expiry = QDate::fromString(query.value(1).toString(), Qt::ISODate);
            batch.quantity = query.value(2).toInt();
            result.append(batch);
        }
    }
    return result;
}

bool InventoryService::receive(int productId, int quantity, const QDate &expiry, QString *error)
{
    if (quantity <= 0) {
        setError(error, QObject::tr("Quantity must be at least 1."));
        return false;
    }

    QSqlDatabase db = m_db.connection();
    if (!db.transaction()) {
        setError(error, db.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    auto fail = [&]() {
        setError(error, query.lastError().text());
        db.rollback();
        return false;
    };

    // 1. A new batch for this delivery.
    query.prepare(QStringLiteral(
        "INSERT INTO batches (product_id, expiry_date, received_at) VALUES (?, ?, ?)"));
    query.addBindValue(productId);
    query.addBindValue(expiry.isValid() ? QVariant(expiry.toString(Qt::ISODate)) : QVariant());
    query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    if (!query.exec())
        return fail();
    const int batchId = query.lastInsertId().toInt();

    // 2. The operation, 3. the movement.
    const int operationId = insertOperation(query, QStringLiteral("receive"));
    if (operationId < 0)
        return fail();
    if (!insertMovement(query, operationId, batchId, quantity))
        return fail();

    if (!db.commit())
        return fail();
    return true;
}

bool InventoryService::issue(int productId, int quantity, QString *error)
{
    if (quantity <= 0) {
        setError(error, QObject::tr("Quantity must be at least 1."));
        return false;
    }

    QSqlDatabase db = m_db.connection();
    if (!db.transaction()) {
        setError(error, db.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    auto fail = [&]() {
        setError(error, query.lastError().text());
        db.rollback();
        return false;
    };

    // Read the batches inside the transaction, so nothing can change
    // between checking the stock and writing the movements.
    const QList<BatchStock> available = batches(productId);
    int total = 0;
    for (const BatchStock &batch : available)
        total += batch.quantity;

    if (total < quantity) {
        db.rollback();
        setError(error, total == 0
                     ? QObject::tr("This product is out of stock.")
                     : QObject::tr("Only %1 in stock.").arg(total));
        return false;
    }

    const int operationId = insertOperation(query, QStringLiteral("issue"));
    if (operationId < 0)
        return fail();

    // Take from the earliest-expiring batch first.
    int remaining = quantity;
    for (const BatchStock &batch : available) {
        const int take = qMin(remaining, batch.quantity);
        if (!insertMovement(query, operationId, batch.batchId, -take))
            return fail();
        remaining -= take;
        if (remaining == 0)
            break;
    }

    if (!db.commit())
        return fail();
    return true;
}

// -------------------------------------------------------- Undo and history

QList<OperationInfo> InventoryService::operations(bool onlyUndoable, int limit) const
{
    QList<OperationInfo> result;
    QSqlQuery query(m_db.connection());

    // One row per stock in/out operation, with the product name and the
    // total number of items. 'undone' is 1 if an undo operation points to it.
    QString sql = QStringLiteral(
        "SELECT o.id, o.kind, o.created_at, p.name, SUM(ABS(m.quantity)), "
        "       EXISTS (SELECT 1 FROM operations u WHERE u.reverses_id = o.id) AS undone "
        "FROM operations o "
        "JOIN movements m ON m.operation_id = o.id "
        "JOIN batches b   ON b.id = m.batch_id "
        "JOIN products p  ON p.id = b.product_id "
        "WHERE o.kind <> 'undo' "
        "GROUP BY o.id ");
    if (onlyUndoable)
        sql += QStringLiteral("HAVING undone = 0 ");
    sql += QStringLiteral("ORDER BY o.id DESC LIMIT ?");

    query.prepare(sql);
    query.addBindValue(limit);
    if (query.exec()) {
        while (query.next()) {
            OperationInfo op;
            op.id = query.value(0).toInt();
            op.kind = query.value(1).toString() == QLatin1String("receive")
                          ? OperationKind::Receive
                          : OperationKind::Issue;
            op.time = QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
            op.productName = query.value(3).toString();
            op.quantity = query.value(4).toInt();
            op.undone = query.value(5).toBool();
            result.append(op);
        }
    }
    return result;
}

std::optional<OperationInfo> InventoryService::lastUndoable() const
{
    const QList<OperationInfo> list = operations(true, 1);
    if (list.isEmpty())
        return std::nullopt;
    return list.first();
}

QList<OperationInfo> InventoryService::recentOperations(int limit) const
{
    return operations(false, limit);
}

bool InventoryService::undoLast(QString *error)
{
    const std::optional<OperationInfo> last = lastUndoable();
    if (!last) {
        setError(error, QObject::tr("There is nothing to undo."));
        return false;
    }

    QSqlDatabase db = m_db.connection();
    if (!db.transaction()) {
        setError(error, db.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    auto fail = [&]() {
        setError(error, query.lastError().text());
        db.rollback();
        return false;
    };

    const int undoId = insertOperation(query, QStringLiteral("undo"), last->id);
    if (undoId < 0)
        return fail();

    // Copy every movement of the original operation with the opposite sign.
    query.prepare(QStringLiteral(
        "INSERT INTO movements (operation_id, batch_id, quantity) "
        "SELECT ?, batch_id, -quantity FROM movements WHERE operation_id = ?"));
    query.addBindValue(undoId);
    query.addBindValue(last->id);
    if (!query.exec())
        return fail();

    if (!db.commit())
        return fail();
    return true;
}
