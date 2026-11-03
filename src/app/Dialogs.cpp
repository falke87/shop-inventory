#include "Dialogs.h"

#include <QCalendarWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

bool isAllDigits(const QString &text)
{
    for (const QChar c : text) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

} // namespace

namespace Dialogs {

bool editProduct(QWidget *parent, const QString &barcode, QString &name, int &minStock)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(name.isEmpty() ? QObject::tr("New product") : QObject::tr("Edit product"));

    auto *nameEdit = new QLineEdit(name);
    auto *minSpin = new QSpinBox;
    minSpin->setRange(0, 9999);
    minSpin->setValue(minStock);

    auto *form = new QFormLayout;
    form->addRow(QObject::tr("Barcode:"), new QLabel(barcode));
    form->addRow(QObject::tr("Name:"), nameEdit);
    form->addRow(QObject::tr("Warn when stock is at or below:"), minSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);

    // A barcode scanner sends digits followed by Enter. If someone scans
    // while this dialog is open, the digits land in the name field.
    // So: Enter must NOT press OK, and a name made only of digits is refused.
    ok->setAutoDefault(false);
    ok->setDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setAutoDefault(false);

    auto updateOk = [=]() {
        const QString text = nameEdit->text().trimmed();
        ok->setEnabled(!text.isEmpty() && !isAllDigits(text));
    };
    QObject::connect(nameEdit, &QLineEdit::textChanged, &dialog, updateOk);
    updateOk();

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    nameEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted)
        return false;

    name = nameEdit->text().trimmed();
    minStock = minSpin->value();
    return true;
}

bool askExpiryDate(QWidget *parent, QDate &date)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Expiry date"));

    auto *calendar = new QCalendarWidget;
    calendar->setMinimumDate(QDate::currentDate());
    calendar->setSelectedDate(date.isValid() ? date : QDate::currentDate().addMonths(6));
    calendar->setGridVisible(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *noExpiry = buttons->addButton(QObject::tr("No expiry date"),
                                               QDialogButtonBox::ResetRole);

    bool cleared = false;
    QObject::connect(noExpiry, &QPushButton::clicked, &dialog, [&]() {
        cleared = true;
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QObject::tr("Click the expiry date printed on the product:")));
    layout->addWidget(calendar);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    date = cleared ? QDate() : calendar->selectedDate();
    return true;
}

SettingsResult editSettings(QWidget *parent, int &expiryWarningDays, QString &backupFolder)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Settings"));

    auto *daysSpin = new QSpinBox;
    daysSpin->setRange(1, 365);
    daysSpin->setValue(expiryWarningDays);
    daysSpin->setSuffix(QObject::tr(" days"));

    auto *folderEdit = new QLineEdit(backupFolder);
    folderEdit->setPlaceholderText(QObject::tr("Default folder on this computer"));
    auto *browse = new QPushButton(QObject::tr("Choose..."));
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&]() {
        const QString folder = QFileDialog::getExistingDirectory(&dialog, QObject::tr("Backup folder"),
                                                                 folderEdit->text());
        if (!folder.isEmpty())
            folderEdit->setText(folder);
    });

    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(folderEdit, 1);
    folderRow->addWidget(browse);

    auto *hint = new QLabel(QObject::tr(
        "Tip: choose a USB stick or a cloud folder (OneDrive, Google Drive).\n"
        "A backup on the same disk does not help if the computer breaks."));
    hint->setWordWrap(true);

    auto *form = new QFormLayout;
    form->addRow(QObject::tr("Warn about expiry this many days before:"), daysSpin);
    form->addRow(QObject::tr("Backup folder:"), folderRow);

    // Restoring is rare and replaces everything, so it lives here,
    // not on the main window.
    auto *restoreButton = new QPushButton(QObject::tr("Restore from backup..."));
    bool restoreRequested = false;
    QObject::connect(restoreButton, &QPushButton::clicked, &dialog, [&]() {
        restoreRequested = true;
        dialog.accept();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(restoreButton, 0, Qt::AlignLeft);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return SettingsResult::Cancelled;
    if (restoreRequested)
        return SettingsResult::RestoreRequested;

    expiryWarningDays = daysSpin->value();
    backupFolder = folderEdit->text().trimmed();
    return SettingsResult::Saved;
}

} // namespace Dialogs
