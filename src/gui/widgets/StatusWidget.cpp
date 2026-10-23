#include "gui/widgets/StatusWidget.h"
#include "gui/components/StatusIndicator.h"
#include <QHBoxLayout>
#include <QPainter>

namespace {
class ElidedMessageLabel final : public QLabel {
public:
    explicit ElidedMessageLabel(StatusIndicator *indicator) : indicator_(indicator) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(0);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        // Le message utilise toujours la même couleur que le voyant, définie en QSS.
        painter.setPen(indicator_->palette().color(indicator_->isEnabled() ? QPalette::Active : QPalette::Disabled,
                                                   QPalette::WindowText));
        const auto area = contentsRect();
        painter.setClipRect(area);
        painter.drawText(area, alignment(),
                         fontMetrics().elidedText(text(), Qt::ElideRight, area.width()));
    }
private:
    StatusIndicator *indicator_;
};
}

StatusWidget::StatusWidget(QWidget *parent) : QWidget(parent) {
    setMinimumWidth(0);
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    indicator_ = new StatusIndicator;
    indicator_->setObjectName("statusindicator");
    message_ = new ElidedMessageLabel(indicator_);
    message_->setObjectName("statuswidget");
    message_->setTextFormat(Qt::PlainText);
    message_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    message_->setWordWrap(false);
    layout->addWidget(message_, 1);
    layout->addWidget(indicator_, 0, Qt::AlignRight);
    updateDisplay();
}

void StatusWidget::setWarning(const QString &text) {
    last_warning = text.trimmed();
    updateDisplay();
}

void StatusWidget::setError(const QString &text) {
    last_error = text.trimmed();
    updateDisplay();
}

void StatusWidget::resetError() {
    last_error.clear();
    updateDisplay();
}

void StatusWidget::resetWarning() {
    last_warning.clear();
    updateDisplay();
}

void StatusWidget::reset() {
    last_error.clear();
    last_warning.clear();
    updateDisplay();
}

void StatusWidget::setUnknown() {
    reset();
    indicator_->setEnabled(false);
    indicator_->setToolTip(tr("État carte inconnu"));
}

StatusIndicator::State StatusWidget::currentState() const {
    if (!last_error.isEmpty()) return StatusIndicator::State::Error;
    if (!last_warning.isEmpty()) return StatusIndicator::State::Warning;
    return StatusIndicator::State::Ok;
}

void StatusWidget::updateDisplay() {
    const auto state = currentState();
    const bool showMessage = state != StatusIndicator::State::Ok;
    QString message = state == StatusIndicator::State::Error ? last_error : last_warning;
    if (!message.isEmpty()) message[0] = message.at(0).toUpper();
    indicator_->setEnabled(true);
    indicator_->setState(state);
    indicator_->setToolTip(showMessage ? message : tr("Aucune erreur signalée"));
    message_->setText(message.simplified());
    message_->setToolTip(message);
    message_->update();
    message_->setVisible(showMessage);
}
