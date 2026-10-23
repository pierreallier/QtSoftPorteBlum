#pragma once
#include <QStyle>
#include <QVariant>
#include <QWidget>

// Qt ne réévalue pas automatiquement les sélecteurs QSS après setProperty().
inline void setStyleProperty(QWidget *widget, const char *name, const QVariant &value) {
    if (widget->property(name) == value) return;
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
