#pragma once

#include <QObject>

class QLineEdit;

// Makes sure no barcode scan gets lost.
//
// The scanner behaves like a keyboard: its "key presses" go to whatever
// widget has the keyboard focus. If the user clicked somewhere else, the
// input field no longer has focus and the scan would disappear.
//
// This filter is installed on the whole application. When a key press
// with text arrives anywhere else in the main window, it moves the focus
// back to the input field and delivers the key there.
class KeyRedirector : public QObject
{
public:
    explicit KeyRedirector(QLineEdit *target, QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLineEdit *m_target;
};
