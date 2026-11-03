#include "Backup.h"

#include "Database.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Backup {

QString fileNameFor(const QDate &day)
{
    return QStringLiteral("inventory-%1.db").arg(day.toString(Qt::ISODate));
}

QString folderFor(const Database &db)
{
    const QString folder = db.setting(QStringLiteral("backup_folder"));
    if (!folder.isEmpty())
        return folder;
    return QFileInfo(db.filePath()).absolutePath() + QStringLiteral("/backups");
}

bool runIfDue(const QSqlDatabase &db, const QString &folder, const QDate &today,
              int keep, QString *error)
{
    QDir dir(folder);
    if (!dir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QObject::tr("Cannot create backup folder %1").arg(folder);
        return false;
    }

    const QString path = dir.filePath(fileNameFor(today));
    if (QFileInfo::exists(path))
        return true;   // already backed up today

    // VACUUM INTO writes a clean, complete copy of the database into a new
    // file. Unlike copying the file directly, it is safe while the
    // database is open, and it is fast for a database of a few megabytes.
    QSqlQuery query(db);
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(QDir::toNativeSeparators(path));
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }

    // File names contain the date as YYYY-MM-DD, so sorting by name
    // sorts by date: the oldest files come first.
    const QStringList files = dir.entryList({QStringLiteral("inventory-*.db")},
                                            QDir::Files, QDir::Name);
    for (int i = 0; i < files.size() - keep; ++i)
        dir.remove(files.at(i));

    return true;
}

} // namespace Backup
