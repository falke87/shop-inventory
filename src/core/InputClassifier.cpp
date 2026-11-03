#include "InputClassifier.h"

namespace {

bool isAllDigits(const QString &text)
{
    if (text.isEmpty())
        return false;
    for (const QChar c : text) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
    }
    return true;
}

} // namespace

namespace Input {

bool isValidGtin(const QString &code)
{
    const int length = code.size();
    if ((length != 8 && length != 12 && length != 13) || !isAllDigits(code))
        return false;

    // Going from right to left, without the check digit itself,
    // the digits are multiplied by 3, 1, 3, 1, ...
    int sum = 0;
    int weight = 3;
    for (int i = length - 2; i >= 0; --i) {
        sum += code.at(i).digitValue() * weight;
        weight = (weight == 3) ? 1 : 3;
    }
    const int expected = (10 - sum % 10) % 10;
    return expected == code.at(length - 1).digitValue();
}

Kind classify(const QString &input)
{
    const QString text = input.trimmed();

    if (text.isEmpty())
        return Kind::Empty;

    if (!isAllDigits(text))
        return Kind::Text;

    if (text.size() <= 4)
        return Kind::Quantity;

    // Only these lengths have a check digit we know how to verify.
    // Other numeric codes are accepted as they are.
    if (text.size() == 8 || text.size() == 12 || text.size() == 13)
        return isValidGtin(text) ? Kind::Barcode : Kind::InvalidBarcode;

    return Kind::Barcode;
}

} // namespace Input
