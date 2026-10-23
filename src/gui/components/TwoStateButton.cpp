#include "gui/components/TwoStateButton.h"
#include "gui/components/StyleHelpers.h"

TwoStateButton::TwoStateButton(QWidget *parent) : QPushButton(parent) {
    setCheckable(true);
    updatePresentation();

}

void TwoStateButton::setStateText(bool state, const QString &text) {
    states_[state].text = text;
    updatePresentation(); updateWidth();
}

void TwoStateButton::setStateIcon(bool state, const QIcon &icon) {
    states_[state].icon = icon;
    updatePresentation(); updateWidth();
}

void TwoStateButton::setStateToolTip(bool state, const QString &tip) {
    states_[state].tip = tip;
    updatePresentation();
}

void TwoStateButton::setChecked(bool checked) {
    QPushButton::setChecked(checked);
    updatePresentation(); // Actualise la presentation
}

void TwoStateButton::nextCheckState() { setChecked(!isChecked()); }

void TwoStateButton::checkStateSet() { updatePresentation(); }

void TwoStateButton::updatePresentation() {
    const auto key = shortcut();
    setText(states_[state()].text);
    setShortcut(key);
    setIcon(states_[state()].icon);
    setToolTip(states_[state()].tip);
    setStyleProperty(this, "running", state());
}

void TwoStateButton::updateWidth() {
    const auto key = shortcut();
    const auto current = state();
    int width = 0;
    for (const auto &entry : states_) {
        setText(entry.text); setIcon(entry.icon);
        width = std::max(width, QPushButton::sizeHint().width());
    }
    setText(states_[current].text); setIcon(states_[current].icon);
    setShortcut(key);
    setFixedWidth(width);
}
