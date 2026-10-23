#include "gui/components/ColorCheckBox.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QFocusEvent>
#include <QMouseEvent>

namespace {
constexpr int contentMargin = 2;
constexpr int focusMargin = 3;
constexpr int labelSpacing = 6;
}

ColorCheckBox::ColorCheckBox(const QString &text, const QColor &color, QWidget *parent) : QCheckBox(text, parent), color_(color) {
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void ColorCheckBox::setColor(const QColor &color) {
    if (!color.isValid() || color_ == color) return;
    color_ = color;
    update();
}

void ColorCheckBox::setDisabledColor(const QColor &color) {
    if (disabledColor_ == color) return;
    disabledColor_ = color;
    update();
}

void ColorCheckBox::setIndicatorSize(int size) {
    size = qMax(1, size);
    if (indicatorSize_ == size) return;
    indicatorSize_ = size;
    updateGeometry();
    update();
}

QSize ColorCheckBox::sizeHint() const {
    // Apply QSS properties before layouts cache the preferred size.
    ensurePolished();
    const QSize label = fontMetrics().size(Qt::TextShowMnemonic, text());
    return QSize(2 * contentMargin + indicatorSize_
                     + (text().isEmpty() ? 0 : labelSpacing + label.width()),
                 2 * contentMargin + qMax(indicatorSize_, fontMetrics().height()));
}

bool ColorCheckBox::hitButton(const QPoint &position) const { return rect().contains(position); }

void ColorCheckBox::focusInEvent(QFocusEvent *event) {
    tabFocus_ = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QCheckBox::focusInEvent(event);
    update();
}

void ColorCheckBox::mousePressEvent(QMouseEvent *event) {
    tabFocus_ = false;
    QCheckBox::mousePressEvent(event);
    update();
}

void ColorCheckBox::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect box = QStyle::visualRect(layoutDirection(), rect(),
                                        QRect(contentMargin, (height() - indicatorSize_) / 2, indicatorSize_, indicatorSize_));
    const QRectF border = QRectF(box).adjusted(1, 1, -1, -1);
    const QColor disabledColor = disabledColor_.isValid() ? disabledColor_
        : palette().color(QPalette::Disabled, QPalette::WindowText);
    const QColor borderColor = isEnabled() ? color_ : disabledColor;
    painter.setPen(QPen(borderColor, indicatorBorderWidth_));
    painter.setBrush(isChecked() ? QBrush(borderColor) : QBrush(Qt::NoBrush));
    painter.drawRoundedRect(border, indicatorRadius_, indicatorRadius_);
    const QColor ink = isChecked()
        ? (borderColor.lightnessF() > 0.6 ? QColor(Qt::black) : QColor(Qt::white))
        : palette().color(QPalette::Disabled, QPalette::WindowText);
    paintSymbol(painter, border, ink);
    const int labelLeft = contentMargin + indicatorSize_ + labelSpacing;
    const QRect label = QStyle::visualRect(layoutDirection(), rect(),
        QRect(labelLeft, contentMargin, qMax(0, width() - labelLeft - contentMargin),
              qMax(0, height() - 2 * contentMargin)));
    style()->drawItemText(&painter, label, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic,
                          palette(), isEnabled(), text(), QPalette::WindowText);
    if (hasFocus() && tabFocus_) {
        const QRect textRect = style()->itemTextRect(fontMetrics(), label,
            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic, isEnabled(), text());
        const QRect focusRect = (text().isEmpty() ? box : textRect)
            .adjusted(-focusMargin, -focusMargin, focusMargin, focusMargin)
            .intersected(rect().adjusted(1, 1, -1, -1));
        // Draw only the outline: native focus primitives may also fill the background.
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter.drawRoundedRect(QRectF(focusRect), 3, 3);
    }
}

void ColorCheckBox::paintSymbol(QPainter &painter, const QRectF &area, const QColor &ink) const {
    if (!isChecked()) return;
    QPainterPath tick;
    tick.moveTo(area.left() + area.width() * 0.22, area.top() + area.height() * 0.50);
    tick.lineTo(area.left() + area.width() * 0.43, area.top() + area.height() * 0.72);
    tick.lineTo(area.left() + area.width() * 0.80, area.top() + area.height() * 0.28);
    painter.setPen(QPen(ink, checkWidth_, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(tick);
}



AxisColorCheckBox::AxisColorCheckBox(const QString &axis, const QColor &color, QWidget *parent)
    : ColorCheckBox({}, color, parent), axis_(axis) {
    setAccessibleName(axis);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void AxisColorCheckBox::paintSymbol(QPainter &painter, const QRectF &area, const QColor &ink) const {
    auto symbolFont = font();
    symbolFont.setPixelSize(qMax(1, indicatorSize() - 8));
    symbolFont.setBold(symbolBold_);
    painter.setFont(symbolFont);
    const QColor disabledInk = disabledSymbolColor_.isValid() ? disabledSymbolColor_
        : palette().color(QPalette::Disabled, QPalette::WindowText);
    painter.setPen(!isChecked() ? uncheckedSymbolColor_
                               : isEnabled() ? ink : disabledInk);
    painter.drawText(area, Qt::AlignCenter, axis_);
}
