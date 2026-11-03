#pragma once

#include <QDate>
#include <QSqlDatabase>
#include <QString>

// Daily backup of the database file.
//
// Simple rule: one file per day, named inventory-YYYY-MM-DD.db.
// If today's file already exists, nothing happens. The program calls
// runIfDue() at start and once an hour, so a shop computer that is
// switched off at night still gets its backup the next morning.
class Database;

namespace Backup {

QString fileNameFor(const QDate &day);

// The folder from the settings, or "backups" next to the database file.
QString folderFor(const Database &db);

// Creates today's backup in 'folder' if it does not exist yet, then keeps
// only the newest 'keep' backup files and deletes older ones.
bool runIfDue(const QSqlDatabase &db, const QString &folder, const QDate &today,
              int keep, QString *error = nullptr);

} // namespace Backup
