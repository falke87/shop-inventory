#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

// Plain data structs passed between the core and the user interface.
// No logic here on purpose.

struct Product
{
    int id = 0;
    QString barcode;
    QString name;
    int minStock = 5;   // warn when stock is at or below this number
};

struct ProductStock
{
    Product product;
    int stock = 0;
};

// What is left of one delivery.
struct BatchStock
{
    int batchId = 0;
    QDate expiry;       // invalid QDate = no expiry date
    int quantity = 0;
};

enum class OperationKind { Receive, Issue };

// One "stock in" or "stock out" action, as shown in the history list.
struct OperationInfo
{
    int id = 0;
    OperationKind kind = OperationKind::Receive;
    QDateTime time;
    QString productName;
    int quantity = 0;    // always positive
    bool undone = false;
};

struct LowStockAlert
{
    QString productName;
    int stock = 0;
    int minStock = 0;
};

struct ExpiryAlert
{
    QString productName;
    QDate expiry;
    int quantity = 0;
    int daysLeft = 0;    // negative = already expired
};
