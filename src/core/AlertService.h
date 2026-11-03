#pragma once

#include <QList>

#include "Types.h"

class Database;

// Read-only queries for the two warning lists.
class AlertService
{
public:
    explicit AlertService(const Database &db);

    // Products whose stock is at or below their minimum.
    QList<LowStockAlert> lowStock() const;

    // Batches that still have goods and expire within 'warningDays'
    // after 'today' (already expired ones included).
    //
    // 'today' is a parameter instead of QDate::currentDate() so that
    // tests give the same result no matter on which day they run.
    QList<ExpiryAlert> expiringSoon(const QDate &today, int warningDays) const;

private:
    const Database &m_db;
};
