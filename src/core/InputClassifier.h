#pragma once

#include <QString>

// Decides what the text in the single input field means.
// Pure functions: no database, no GUI, no clock -> easy to test.
namespace Input {

enum class Kind {
    Empty,           // nothing typed
    Quantity,        // 1 to 4 digits, e.g. "24": quantity for the next scan
    Barcode,         // 5 or more digits
    InvalidBarcode,  // looks like EAN-8 / UPC-A / EAN-13 but the check digit is wrong
    Text,            // anything else: try it as a barcode first, then as a name
};

Kind classify(const QString &input);

// True if 'code' is an EAN-8, UPC-A (12 digits) or EAN-13 code
// with a correct check digit (the last digit).
bool isValidGtin(const QString &code);

} // namespace Input
