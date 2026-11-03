#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>

#include "Backup.h"
#include "Database.h"
#include "KeyRedirector.h"
#include "MainWindow.h"

namespace {

// Big text, strong contrast, big buttons. All visual rules in one place.
const char *styleSheet = R"(
    QWidget              { font-size: 17pt; color: #111; }
    QWidget#root         { background: #ffffff; }
    QPushButton          { background: #eeeeee; border: 2px solid #444; border-radius: 8px;
                           padding: 10px 18px; }
    QPushButton:pressed  { background: #cccccc; }
    QPushButton:disabled { color: #999; border-color: #bbb; }
    QPushButton[role="mode"]          { font-size: 24pt; font-weight: bold; min-height: 80px; }
    QPushButton[role="mode"]:checked  { background: #0d47a1; color: white; border-color: #0d47a1; }
    QPushButton#undoButton            { background: #ffd54f; font-weight: bold; min-height: 64px;
                                        text-align: left; padding-left: 20px; }
    QPushButton#undoButton:disabled   { background: #f3f3f3; }
    QLineEdit#input      { font-size: 26pt; border: 3px solid #0d47a1; border-radius: 6px; padding: 8px; }
    QLabel#heading       { font-weight: bold; margin-top: 8px; }
    QLabel#nextScan      { font-size: 19pt; }
    QListWidget, QTableWidget { border: 1px solid #999; }
    QListWidget::item    { padding: 6px; }
)";

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ShopInventory"));
    app.setOrganizationName(QStringLiteral("ShopInventory"));   // used in the data folder path
    app.setStyleSheet(QString::fromLatin1(styleSheet));

    // Windows: C:/Users/<name>/AppData/Roaming/ShopInventory/ShopInventory
    // (Program Files is read-only for normal users, so data cannot go there.)
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // Allow only one running copy. Two windows writing the same file
    // would show different numbers and confuse the user.
    QLockFile lock(dataDir + QStringLiteral("/inventory.lock"));
    if (!lock.tryLock(100)) {
        QMessageBox::information(nullptr, QObject::tr("Shop Inventory"),
                                 QObject::tr("The program is already open."));
        return 0;
    }

    Database db(dataDir + QStringLiteral("/inventory.db"));
    QString error;
    if (!db.open(&error)) {
        QMessageBox::critical(nullptr, QObject::tr("Shop Inventory"), error);
        return 1;
    }

    MainWindow window(db);
    window.setObjectName(QStringLiteral("root"));

    KeyRedirector redirector(window.input());
    app.installEventFilter(&redirector);

    // Backup: once at start, then checked every hour (a new day may have begun).
    auto runBackup = [&]() {
        QString backupError;
        if (!Backup::runIfDue(db.connection(), Backup::folderFor(db), QDate::currentDate(), 30,
                              &backupError))
            window.showMessage(QObject::tr("Backup failed: %1").arg(backupError),
                               MainWindow::MessageType::Error);
    };
    runBackup();

    QTimer hourly;
    QObject::connect(&hourly, &QTimer::timeout, &window, [&]() {
        runBackup();
        window.refresh();   // expiry warnings change when the day changes
    });
    hourly.start(60 * 60 * 1000);

    window.showMaximized();
    window.input()->setFocus();
    return app.exec();
}
