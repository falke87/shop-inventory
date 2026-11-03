# Assumptions

This is a practice project for an imagined, ordinary Asian grocery shop.
These assumptions replace the questions one would ask a real customer.

| Topic | Assumption | Effect on the design |
|---|---|---|
| Stock out | Each item that leaves the shop is scanned once. No separate till system. | Scan = written. No extra step. |
| Expiry dates | Exact day. The same product can arrive with different dates. | Every delivery is a **batch**. Stock out takes from the batch that expires first. |
| Barcodes | Every product has exactly one barcode. No carton codes. | `products.barcode` is `UNIQUE`; no separate barcode table. |
| Loose goods | Not handled in v1. | Out of scope. |
| First data | No import. Scanning an unknown barcode in "Stock in" opens a "New product" form. | No CSV import needed. |
| Count differences | Missing goods: "Stock out". Extra goods: "Stock in". | No extra feature. |
| Computer | Windows 10/11, 64-bit, mouse + USB scanner, no touch screen. | Qt 6. |
| Users | One person, no login. | No user table. |
| Language | English. | Strings wrapped in `tr()`, no translation files. |
| Low stock | Per product, default 5. | `products.min_stock`. |
| Expiry warning | One number for all, default 14 days. | `settings` table. |
| Undo | Each press undoes the newest action not undone yet. | Undo operations in the ledger. |
| Backup | Once a day, newest 30 kept, folder can be a USB stick or cloud folder. | `VACUUM INTO` at start and every hour. |
| Restore | From inside the program (Settings), current data kept as a safety copy. | `Database::restoreFrom()`. |
| Scanner broken / label torn | Type the barcode, or type part of the name and click the product. | One input field for everything. |
