#include "MainWindow.h"

#include "Backup.h"
#include "Database.h"
#include "Dialogs.h"
#include "InputClassifier.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// Buttons and lists never take keyboard focus. That way the keyboard
// focus stays in the input field, where the scanner's "typing" must go.
QPushButton *makeButton(const QString &text, const QString &objectName = QString())
{
    auto *button = new QPushButton(text);
    button->setFocusPolicy(Qt::NoFocus);
    button->setObjectName(objectName);
    return button;
}

QListWidget *makeList()
{
    auto *list = new QListWidget;
    list->setFocusPolicy(Qt::NoFocus);
    list->setWordWrap(true);   // long lines wrap instead of needing a side scroll bar
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return list;
}

QLabel *makeHeading(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("heading"));
    return label;
}

QString pieces(int n)
{
    return n == 1 ? QObject::tr("1 piece") : QObject::tr("%1 pieces").arg(n);
}

QString describe(const OperationInfo &op)
{
    return op.kind == OperationKind::Receive
               ? QObject::tr("In: %1 × %2").arg(op.quantity).arg(op.productName)
               : QObject::tr("Out: %1 × %2").arg(op.quantity).arg(op.productName);
}

QString dateText(const QDate &date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy"))
                          : QObject::tr("no expiry date");
}

} // namespace

MainWindow::MainWindow(Database &db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_inventory(db)
    , m_alerts(db)
{
    setWindowTitle(tr("Shop Inventory"));

    // ---- Top row: the three big mode buttons ----
    m_receiveButton = makeButton(tr("Stock in"), QStringLiteral("stockInButton"));
    m_issueButton = makeButton(tr("Stock out"), QStringLiteral("stockOutButton"));
    m_lookupButton = makeButton(tr("Look up"), QStringLiteral("lookupButton"));

    auto *modeGroup = new QButtonGroup(this);   // only one can be pressed at a time
    for (QPushButton *button : {m_receiveButton, m_issueButton, m_lookupButton}) {
        button->setCheckable(true);
        button->setProperty("role", "mode");
        modeGroup->addButton(button);
    }
    connect(m_receiveButton, &QPushButton::clicked, this, [this] { setMode(Mode::Receive); });
    connect(m_issueButton, &QPushButton::clicked, this, [this] { setMode(Mode::Issue); });
    connect(m_lookupButton, &QPushButton::clicked, this, [this] { setMode(Mode::Lookup); });

    auto *settingsButton = makeButton(tr("Settings"));
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(m_receiveButton, 1);
    topRow->addWidget(m_issueButton, 1);
    topRow->addWidget(m_lookupButton, 1);
    topRow->addSpacing(20);
    topRow->addWidget(settingsButton);

    // ---- Input field and message line ----
    m_input = new QLineEdit;
    m_input->setObjectName(QStringLiteral("input"));
    m_input->setPlaceholderText(tr("Scan, or type barcode / name + Enter"));
    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::handleInput);

    m_message = new QLabel;
    m_message->setObjectName(QStringLiteral("message"));
    m_message->setWordWrap(true);
    m_message->setMinimumHeight(70);

    // ---- Page 1: stock in / stock out ----
    m_nextScanLabel = new QLabel;
    m_nextScanLabel->setObjectName(QStringLiteral("nextScan"));
    m_expiryButton = makeButton(tr("Set expiry date"));
    connect(m_expiryButton, &QPushButton::clicked, this, &MainWindow::chooseExpiryDate);
    auto *resetButton = makeButton(tr("Reset"));
    connect(resetButton, &QPushButton::clicked, this, [this] {
        resetNextScan();
        showMessage(tr("Next scan reset to 1 piece."), MessageType::Info);
    });

    auto *nextScanRow = new QHBoxLayout;
    nextScanRow->addWidget(m_nextScanLabel, 1);
    nextScanRow->addWidget(m_expiryButton);
    nextScanRow->addWidget(resetButton);

    m_resultsLabel = makeHeading(tr("Found - click the right product:"));
    m_results = makeList();
    m_results->setObjectName(QStringLiteral("results"));
    connect(m_results, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (auto product = m_inventory.findByBarcode(item->data(Qt::UserRole).toString()))
            actOnProduct(*product);
    });

    m_recent = makeList();

    auto *workPage = new QWidget;
    auto *workLayout = new QVBoxLayout(workPage);
    workLayout->setContentsMargins(0, 0, 0, 0);
    workLayout->addLayout(nextScanRow);
    workLayout->addWidget(m_resultsLabel);
    workLayout->addWidget(m_results, 1);
    workLayout->addWidget(makeHeading(tr("Recent")));
    workLayout->addWidget(m_recent, 1);

    // ---- Page 2: look up ----
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Barcode"), tr("In stock"), tr("Minimum")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &MainWindow::showSelectedProductDetails);

    m_details = new QLabel;
    m_details->setWordWrap(true);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_editButton = makeButton(tr("Edit product"));
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editSelectedProduct);

    auto *lookupPage = new QWidget;
    auto *lookupLayout = new QVBoxLayout(lookupPage);
    lookupLayout->setContentsMargins(0, 0, 0, 0);
    lookupLayout->addWidget(m_table, 3);
    lookupLayout->addWidget(m_details, 1);
    lookupLayout->addWidget(m_editButton, 0, Qt::AlignLeft);

    m_pages = new QStackedWidget;
    m_pages->addWidget(workPage);     // index 0
    m_pages->addWidget(lookupPage);   // index 1

    // ---- Undo ----
    m_undoButton = makeButton(tr("Undo"), QStringLiteral("undoButton"));
    connect(m_undoButton, &QPushButton::clicked, this, &MainWindow::undo);

    // ---- Right side: warnings ----
    m_lowLabel = makeHeading(QString());
    m_lowList = makeList();
    m_expiryLabel = makeHeading(QString());
    m_expiryList = makeList();

    auto *alertsLayout = new QVBoxLayout;
    alertsLayout->addWidget(m_lowLabel);
    alertsLayout->addWidget(m_lowList, 1);
    alertsLayout->addWidget(m_expiryLabel);
    alertsLayout->addWidget(m_expiryList, 1);

    // ---- Put it all together ----
    auto *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(m_input);
    leftLayout->addWidget(m_message);
    leftLayout->addWidget(m_pages, 1);
    leftLayout->addWidget(m_undoButton);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addLayout(leftLayout, 2);
    bottomRow->addSpacing(20);
    bottomRow->addLayout(alertsLayout, 1);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topRow);
    mainLayout->addLayout(bottomRow, 1);

    setMode(Mode::Receive);
    refresh();
}

// ------------------------------------------------------------------ Modes

void MainWindow::setMode(Mode mode)
{
    m_mode = mode;
    m_receiveButton->setChecked(mode == Mode::Receive);
    m_issueButton->setChecked(mode == Mode::Issue);
    m_lookupButton->setChecked(mode == Mode::Lookup);

    m_pages->setCurrentIndex(mode == Mode::Lookup ? 1 : 0);
    m_expiryButton->setVisible(mode == Mode::Receive);
    m_results->clear();
    m_resultsLabel->hide();
    m_results->hide();
    resetNextScan();

    switch (mode) {
    case Mode::Receive:
        showMessage(tr("STOCK IN: scan each product. For many at once, type the number "
                       "(e.g. 24), press Enter, then scan."), MessageType::Info);
        break;
    case Mode::Issue:
        showMessage(tr("STOCK OUT: scan each product that leaves the shop."), MessageType::Info);
        break;
    case Mode::Lookup:
        fillTable(QString());
        showMessage(tr("LOOK UP: scan a product or type part of its name."), MessageType::Info);
        break;
    }
    m_input->setFocus();
}

// ------------------------------------------------------------ Input field

void MainWindow::handleInput()
{
    const QString text = m_input->text().trimmed();
    m_input->clear();

    switch (Input::classify(text)) {
    case Input::Kind::Empty:
        if (m_mode == Mode::Lookup)
            fillTable(QString());   // Enter on an empty field shows everything again
        return;

    case Input::Kind::Quantity:
        if (m_mode != Mode::Lookup) {
            const int n = text.toInt();
            if (n <= 0) {
                showMessage(tr("The quantity must be at least 1."), MessageType::Error);
                return;
            }
            m_nextQuantity = n;
            updateNextScanLabel();
            showMessage(tr("OK, the next scan counts as %1.").arg(pieces(n)), MessageType::Info);
            return;
        }
        break;   // in look-up mode a short number is just a search text

    case Input::Kind::InvalidBarcode:
        showMessage(tr("\"%1\" is not a valid barcode. Please check the number.").arg(text),
                    MessageType::Error);
        return;

    case Input::Kind::Barcode:
    case Input::Kind::Text:
        break;
    }

    // Barcode or text: an exact barcode match always wins.
    if (std::optional<Product> product = m_inventory.findByBarcode(text)) {
        actOnProduct(*product);
        return;
    }

    if (Input::classify(text) == Input::Kind::Barcode)
        handleUnknownBarcode(text);
    else
        showSearchResults(text);
}

void MainWindow::actOnProduct(const Product &product)
{
    m_results->clear();
    m_resultsLabel->hide();
    m_results->hide();

    QString error;
    switch (m_mode) {
    case Mode::Receive:
        if (m_inventory.receive(product.id, m_nextQuantity, m_nextExpiry, &error)) {
            showMessage(tr("+%1   %2   (now %3 in stock)")
                            .arg(m_nextQuantity)
                            .arg(product.name)
                            .arg(m_inventory.stock(product.id)),
                        MessageType::Success);
            resetNextScan();
        } else {
            showMessage(error, MessageType::Error);
        }
        break;

    case Mode::Issue:
        if (m_inventory.issue(product.id, m_nextQuantity, &error)) {
            showMessage(tr("-%1   %2   (%3 left)")
                            .arg(m_nextQuantity)
                            .arg(product.name)
                            .arg(m_inventory.stock(product.id)),
                        MessageType::Success);
            resetNextScan();
        } else {
            showMessage(tr("%1: %2").arg(product.name, error), MessageType::Error);
        }
        break;

    case Mode::Lookup:
        fillTable(QString());
        selectProductInTable(product.id);
        showMessage(product.name, MessageType::Info);
        break;
    }
    refresh();
}

void MainWindow::handleUnknownBarcode(const QString &barcode)
{
    switch (m_mode) {
    case Mode::Receive: {
        QString name;
        int minStock = 5;
        if (!Dialogs::editProduct(this, barcode, name, minStock)) {
            showMessage(tr("Cancelled. Nothing was saved."), MessageType::Info);
            return;
        }
        QString error;
        if (m_inventory.addProduct(barcode, name, minStock, &error) < 0) {
            showMessage(error, MessageType::Error);
            return;
        }
        if (std::optional<Product> product = m_inventory.findByBarcode(barcode))
            actOnProduct(*product);   // the new product is now stocked in
        break;
    }
    case Mode::Issue:
        showMessage(tr("Unknown barcode %1. Use \"Stock in\" to add this product first.").arg(barcode),
                    MessageType::Error);
        break;
    case Mode::Lookup:
        showMessage(tr("No product with barcode %1.").arg(barcode), MessageType::Error);
        break;
    }
}

void MainWindow::showSearchResults(const QString &text)
{
    if (m_mode == Mode::Lookup) {
        fillTable(text);
        if (m_table->rowCount() == 0)
            showMessage(tr("Nothing found for \"%1\".").arg(text), MessageType::Error);
        else
            showMessage(tr("Products matching \"%1\". Press Enter on an empty field to see all.")
                            .arg(text), MessageType::Info);
        return;
    }

    const QList<Product> products = m_inventory.searchByName(text);
    m_results->clear();
    if (products.isEmpty()) {
        m_resultsLabel->hide();
        m_results->hide();
        showMessage(tr("Nothing found for \"%1\".").arg(text), MessageType::Error);
        return;
    }

    // The user must click the product: guessing could pick the wrong one.
    for (const Product &product : products) {
        auto *item = new QListWidgetItem(product.name, m_results);
        item->setData(Qt::UserRole, product.barcode);
    }
    m_resultsLabel->show();
    m_results->show();
    showMessage(tr("Click the right product in the list."), MessageType::Info);
}

// ---------------------------------------------------------------- Undo

void MainWindow::undo()
{
    const std::optional<OperationInfo> last = m_inventory.lastUndoable();
    if (!last)
        return;

    QString error;
    if (m_inventory.undoLast(&error))
        showMessage(tr("Undone: %1").arg(describe(*last)), MessageType::Info);
    else
        showMessage(error, MessageType::Error);
    refresh();
}

// ----------------------------------------------------------- Next scan

void MainWindow::resetNextScan()
{
    m_nextQuantity = 1;
    m_nextExpiry = QDate();
    updateNextScanLabel();
}

void MainWindow::updateNextScanLabel()
{
    if (m_mode == Mode::Receive)
        m_nextScanLabel->setText(tr("Next scan: %1, %2").arg(pieces(m_nextQuantity), dateText(m_nextExpiry)));
    else
        m_nextScanLabel->setText(tr("Next scan: %1").arg(pieces(m_nextQuantity)));
}

void MainWindow::chooseExpiryDate()
{
    QDate date = m_nextExpiry;
    if (Dialogs::askExpiryDate(this, date)) {
        m_nextExpiry = date;
        updateNextScanLabel();
        showMessage(tr("Expiry date for the next scan: %1. Now scan the product.").arg(dateText(date)),
                    MessageType::Info);
    }
    m_input->setFocus();
}

// -------------------------------------------------------------- Look up

void MainWindow::fillTable(const QString &filter)
{
    m_tableFilter = filter;
    m_table->setRowCount(0);

    for (const ProductStock &row : m_inventory.allProducts()) {
        if (!filter.isEmpty() && !row.product.name.contains(filter, Qt::CaseInsensitive))
            continue;

        const int r = m_table->rowCount();
        m_table->insertRow(r);

        auto *nameItem = new QTableWidgetItem(row.product.name);
        nameItem->setData(Qt::UserRole, row.product.id);
        m_table->setItem(r, 0, nameItem);
        m_table->setItem(r, 1, new QTableWidgetItem(row.product.barcode));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::number(row.stock)));
        m_table->setItem(r, 3, new QTableWidgetItem(QString::number(row.product.minStock)));

        if (row.stock <= row.product.minStock)
            m_table->item(r, 2)->setForeground(QColor(QStringLiteral("#b00020")));
    }

    if (m_table->rowCount() == 1)
        m_table->selectRow(0);
    showSelectedProductDetails();
}

void MainWindow::selectProductInTable(int productId)
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->data(Qt::UserRole).toInt() == productId) {
            m_table->selectRow(r);
            m_table->scrollToItem(m_table->item(r, 0));
            return;
        }
    }
}

std::optional<Product> MainWindow::selectedProduct() const
{
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty())
        return std::nullopt;
    const int row = selected.first()->row();
    return m_inventory.findByBarcode(m_table->item(row, 1)->text());
}

void MainWindow::showSelectedProductDetails()
{
    const std::optional<Product> product = selectedProduct();
    m_editButton->setEnabled(product.has_value());
    if (!product) {
        m_details->setText(tr("Select a product to see its batches."));
        return;
    }

    QStringList lines;
    lines << tr("%1 - %2 in stock").arg(product->name).arg(m_inventory.stock(product->id));
    const QList<BatchStock> batches = m_inventory.batches(product->id);
    if (batches.isEmpty())
        lines << tr("No goods left.");
    for (const BatchStock &batch : batches)
        lines << tr("   %1, %2").arg(pieces(batch.quantity), dateText(batch.expiry));
    m_details->setText(lines.join(QLatin1Char('\n')));
}

void MainWindow::editSelectedProduct()
{
    const std::optional<Product> product = selectedProduct();
    if (!product)
        return;

    QString name = product->name;
    int minStock = product->minStock;
    if (Dialogs::editProduct(this, product->barcode, name, minStock)) {
        QString error;
        if (m_inventory.updateProduct(product->id, name, minStock, &error))
            showMessage(tr("Saved: %1").arg(name), MessageType::Success);
        else
            showMessage(error, MessageType::Error);
        fillTable(m_tableFilter);
        selectProductInTable(product->id);
        refresh();
    }
    m_input->setFocus();
}

// -------------------------------------------------------------- Settings

void MainWindow::openSettings()
{
    int days = m_db.setting(QStringLiteral("expiry_warning_days"), QStringLiteral("14")).toInt();
    QString folder = m_db.setting(QStringLiteral("backup_folder"));

    switch (Dialogs::editSettings(this, days, folder)) {
    case Dialogs::SettingsResult::Cancelled:
        break;
    case Dialogs::SettingsResult::Saved:
        m_db.setSetting(QStringLiteral("expiry_warning_days"), QString::number(days));
        m_db.setSetting(QStringLiteral("backup_folder"), folder);
        showMessage(tr("Settings saved."), MessageType::Success);
        refresh();
        break;
    case Dialogs::SettingsResult::RestoreRequested:
        restoreFromBackup();
        break;
    }
    m_input->setFocus();
}

void MainWindow::restoreFromBackup()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose a backup"),
                                                      Backup::folderFor(m_db),
                                                      tr("Backups (*.db)"));
    if (path.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this, tr("Restore from backup"),
        tr("Replace ALL current data with this backup?\n\n%1\n\n"
           "Your current data is kept as a safety copy.")
            .arg(QFileInfo(path).fileName()));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (m_db.restoreFrom(path, &error)) {
        setMode(m_mode);   // reload the page that is shown
        refresh();
        showMessage(tr("Restored from %1.").arg(QFileInfo(path).fileName()), MessageType::Success);
    } else {
        showMessage(tr("Restore failed: %1").arg(error), MessageType::Error);
    }
}

// ---------------------------------------------------------- Refresh / UI

void MainWindow::refresh()
{
    // Undo button shows exactly what it will undo.
    if (const std::optional<OperationInfo> last = m_inventory.lastUndoable()) {
        m_undoButton->setText(tr("Undo:  %1").arg(describe(*last)));
        m_undoButton->setEnabled(true);
    } else {
        m_undoButton->setText(tr("Undo"));
        m_undoButton->setEnabled(false);
    }

    // History
    m_recent->clear();
    for (const OperationInfo &op : m_inventory.recentOperations(10)) {
        QString text = op.time.toString(QStringLiteral("HH:mm")) + QStringLiteral("   ") + describe(op);
        auto *item = new QListWidgetItem(m_recent);
        if (op.undone) {
            text += tr("   (undone)");
            item->setForeground(Qt::gray);
        }
        item->setText(text);
    }

    // Warnings: low stock
    const QList<LowStockAlert> low = m_alerts.lowStock();
    m_lowLabel->setText(tr("Running low (%1)").arg(low.size()));
    m_lowList->clear();
    for (const LowStockAlert &alert : low)
        m_lowList->addItem(tr("%1: %2 left").arg(alert.productName).arg(alert.stock));

    // Warnings: expiry
    const int days = m_db.setting(QStringLiteral("expiry_warning_days"), QStringLiteral("14")).toInt();
    const QList<ExpiryAlert> expiring = m_alerts.expiringSoon(QDate::currentDate(), days);
    m_expiryLabel->setText(tr("Expiring soon (%1)").arg(expiring.size()));
    m_expiryList->clear();
    for (const ExpiryAlert &alert : expiring) {
        QString when;
        if (alert.daysLeft < 0)
            when = tr("EXPIRED %1").arg(dateText(alert.expiry));
        else if (alert.daysLeft == 0)
            when = tr("expires TODAY");
        else
            when = tr("in %1 days (%2)").arg(alert.daysLeft).arg(dateText(alert.expiry));

        auto *item = new QListWidgetItem(tr("%1 × %2: %3").arg(alert.quantity).arg(alert.productName, when),
                                         m_expiryList);
        if (alert.daysLeft <= 0)
            item->setForeground(QColor(QStringLiteral("#b00020")));
    }

    if (m_mode == Mode::Lookup) {
        const std::optional<Product> selected = selectedProduct();
        fillTable(m_tableFilter);
        if (selected)
            selectProductInTable(selected->id);
    }
}

void MainWindow::showMessage(const QString &text, MessageType type)
{
    QString style;
    switch (type) {
    case MessageType::Info:
        style = QStringLiteral("background: #e3ecf8; color: #0b2a55;");
        break;
    case MessageType::Success:
        style = QStringLiteral("background: #1b5e20; color: white; font-weight: bold;");
        break;
    case MessageType::Error:
        style = QStringLiteral("background: #b00020; color: white; font-weight: bold;");
        QApplication::beep();   // the user may not be looking at the screen while scanning
        break;
    }
    m_message->setStyleSheet(style + QStringLiteral(" padding: 10px; border-radius: 6px;"));
    m_message->setText(text);
}
