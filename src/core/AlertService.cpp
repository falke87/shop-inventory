#include "AlertService.h"

#include "Database.h"

#include <QSqlQuery>
#include <QVariant>

AlertService::AlertService(const Database &db)
    : m_db(db)
{
}

QList<LowStockAlert> AlertService::lowStock() const
{
    QList<LowStockAlert> result;
    QSqlQuery query(m_db.connection());
    const bool ok = query.exec(QStringLiteral(
        "SELECT p.name, COALESCE(SUM(m.quantity), 0) AS stock, p.min_stock "
        "FROM products p "
        "LEFT JOIN batches b ON b.product_id = p.id "
        "LEFT JOIN movements m ON m.batch_id = b.id "
        "GROUP BY p.id "
        "HAVING stock <= p.min_stock "
        "ORDER BY stock, p.name COLLATE NOCASE"));
    if (ok) {
        while (query.next())
            result.append({query.value(0).toString(), query.value(1).toInt(), query.value(2).toInt()});
    }
    return result;
}

QList<ExpiryAlert> AlertService::expiringSoon(const QDate &today, int warningDays) const
{
    QList<ExpiryAlert> result;
    QSqlQuery query(m_db.connection());
    // Dates are 'YYYY-MM-DD' text, so "<=" on text compares them correctly.
    query.prepare(QStringLiteral(
        "SELECT p.name, b.expiry_date, SUM(m.quantity) AS qty "
        "FROM batches b "
        "JOIN products p ON p.id = b.product_id "
        "JOIN movements m ON m.batch_id = b.id "
        "WHERE b.expiry_date IS NOT NULL AND b.expiry_date <= ? "
        "GROUP BY b.id "
        "HAVING qty > 0 "
        "ORDER BY b.expiry_date, p.name COLLATE NOCASE"));
    query.addBindValue(today.addDays(warningDays).toString(Qt::ISODate));
    if (query.exec()) {
        while (query.next()) {
            ExpiryAlert alert;
            alert.productName = query.value(0).toString();
            alert.expiry = QDate::fromString(query.value(1).toString(), Qt::ISODate);
            alert.quantity = query.value(2).toInt();
            alert.daysLeft = static_cast<int>(today.daysTo(alert.expiry));
            result.append(alert);
        }
    }
    return result;
}
