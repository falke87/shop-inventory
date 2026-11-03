#include "KeyRedirector.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>

KeyRedirector::KeyRedirector(QLineEdit *target, QObject *parent)
    : QObject(parent)
    , m_target(target)
{
}

bool KeyRedirector::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return false;   // not a key press: let Qt handle it normally

    // A dialog is open (new product, settings, ...): keys belong to it.
    if (QApplication::activeModalWidget())
        return false;

    QWidget *focus = QApplication::focusWidget();
    if (watched == m_target || focus == m_target)
        return false;   // already going to the right place

    // Do not steal keys from other text fields, if there ever are any.
    if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus))
        return false;

    auto *key = static_cast<QKeyEvent *>(event);
    if (key->text().isEmpty())
        return false;   // Shift, arrow keys, F-keys...: nothing to type

    // Send a copy of the key press to the input field.
    m_target->setFocus();
    QKeyEvent copy(key->type(), key->key(), key->modifiers(), key->text());
    QApplication::sendEvent(m_target, &copy);
    return true;   // the original event is done
}
