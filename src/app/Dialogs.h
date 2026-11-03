#pragma once

#include <QDate>
#include <QString>

class QWidget;

// Small modal dialogs. Each one is a plain function:
// it shows the dialog, and returns true if the user pressed OK.
// The values are passed in and out by reference.
namespace Dialogs {

// New product (name empty) or edit product (name filled in).
bool editProduct(QWidget *parent, const QString &barcode, QString &name, int &minStock);

// Choose an expiry date. On "No expiry date", 'date' becomes invalid.
bool askExpiryDate(QWidget *parent, QDate &date);

// The settings dialog can end in three ways.
enum class SettingsResult { Cancelled, Saved, RestoreRequested };

SettingsResult editSettings(QWidget *parent, int &expiryWarningDays, QString &backupFolder);

} // namespace Dialogs
