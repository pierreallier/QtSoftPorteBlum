#include "serial/Protocol.h"
#include "gui/widgets/PIDWidget.h"
#include <QLocale>
#include <QCheckBox>
#include <QComboBox>
#include <QListView>
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

namespace {
QDoubleSpinBox *spin(const QString &name, double value) {
    auto field = new QDoubleSpinBox;
    field->setObjectName(name);
    field->setRange(0, 100);
    field->setDecimals(0);
    field->setValue(value);
    return field;
}
}

PIDWidget::PIDWidget(const QString &title, QWidget *parent) : QWidget(parent) {
    setLayout(makeUI(title));
    connect(active_, &QCheckBox::toggled, this, [this](bool checked) {
        updateControls(); emit toggled(checked);
    });
    connect(type_, &QComboBox::currentTextChanged, this, &PIDWidget::updateControls);
    connect(help_, &QToolButton::clicked, this, [this] {
        QToolTip::showText(help_->mapToGlobal(help_->rect().bottomLeft()), help_->toolTip(), help_);
    });
    updateControls();
}

QVBoxLayout *PIDWidget::makeUI(const QString &title) {
    auto layout = new QVBoxLayout(this);
    const auto margins = layout->contentsMargins();
    layout->setContentsMargins(3, margins.top(), 3, margins.bottom());

    active_ = new QCheckBox(title);
    active_->setObjectName("pidActif");
    active_->setChecked(true);
    auto header = new QHBoxLayout;
    header->setSpacing(4);
    header->addWidget(active_);

    help_ = new QToolButton;
    help_->setAutoRaise(true);
    help_->setIcon(style()->standardIcon(QStyle::SP_MessageBoxQuestion));
    help_->setAccessibleName(tr("Aide sur le correcteur"));
    header->addWidget(help_);
    header->addStretch();
    layout->addLayout(header);

    type_ = new QComboBox;
    type_->setFocusPolicy(Qt::TabFocus);
    type_->setView(new QListView);
    type_->setItemDelegate(new QStyledItemDelegate(type_));
    type_->setObjectName("typePid");
    type_->addItems({"P", "PI", "PID"});

    auto form = new QGridLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setColumnStretch(1, 1);
    form->addWidget(type_, 0, 0, 1, 2);
    
    kp_ = spin("kp", 1); ki_ = spin("ki", 0); kd_ = spin("kd", 0);
    form->addWidget(new QLabel("K<sub>p</sub>"), 1, 0);
    form->addWidget(kp_, 1, 1);
    kiLabel_ = new QLabel("K<sub>i</sub>");
    kdLabel_ = new QLabel("K<sub>d</sub>");
    form->addWidget(kiLabel_, 2, 0);
    form->addWidget(ki_, 2, 1);
    form->addWidget(kdLabel_, 3, 0);
    form->addWidget(kd_, 3, 1);
    layout->addLayout(form);
    return layout;
}

bool PIDWidget::isChecked() const { return active_->isChecked(); }
void PIDWidget::setChecked(bool checked) { active_->setChecked(checked); }

void PIDWidget::updateControls() {
    const int type = type_->currentIndex();
    type_->setEnabled(isChecked());
    kp_->setEnabled(isChecked());
    ki_->setEnabled(isChecked() && type >= 1); 
    kiLabel_->setEnabled(isChecked() && type >= 1);
    kd_->setEnabled(isChecked() && type == 2); 
    kdLabel_->setEnabled(isChecked() && type == 2);
    makeHelp();
}

void PIDWidget::makeHelp() {
    const int type = type_->currentIndex();
    QString formula = "C(p) = K<sub>p</sub>";
    if (type >= 1) formula += " + K<sub>i</sub>/p";
    if (type == 2) formula += " + K<sub>d</sub>·p";
    QString gains = tr("K<sub>p</sub> : gain proportionnel");
    if (type >= 1) gains += tr("<br>K<sub>i</sub> : gain intégral");
    if (type == 2) gains += tr("<br>K<sub>d</sub> : gain dérivé");
    help_->setToolTip(tr("<b>Correcteur %1</b><br><br>%2<br><br>%3")
                         .arg(type_->currentText(), formula, gains));
}

QString PIDWidget::description() const {
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    const auto values = getCommand(Protocol::CMD::SetPidVitesse).first().values;
    return QString("(Kp=%1, Ki=%2, Kd=%3)")
        .arg(locale.toString(values.value(0).toDouble(), 'f', 2),
             locale.toString(values.value(1).toDouble(), 'f', 2),
             locale.toString(values.value(2).toDouble(), 'f', 2));
}

QVector<Protocol::CommandRequest> PIDWidget::getCommand(Protocol::CMD cmd) const {
    return {{cmd, {QString::number(kp_->value()),
                   QString::number(type_->currentIndex() >= 1 ? ki_->value() : 0),
                   QString::number(type_->currentIndex() == 2 ? kd_->value() : 0)}}};
}
