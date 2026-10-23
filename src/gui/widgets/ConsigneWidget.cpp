#include "serial/Protocol.h"
#include "gui/widgets/ConsigneWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QListView>
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QToolTip>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <algorithm>
#include <cmath>

namespace {
class PointDelegate final : public QStyledItemDelegate {
public:
    PointDelegate(QDoubleSpinBox *amplitude, QObject *parent)
        : QStyledItemDelegate(parent), amplitude_(amplitude) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const override {
        auto spin = new QDoubleSpinBox(parent);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setFrame(false);
        spin->setKeyboardTracking(false);
        spin->setDecimals(index.column() == 0 ? 2 : amplitude_->decimals());
        const double limit = std::max(std::abs(amplitude_->minimum()), std::abs(amplitude_->maximum()));
        spin->setRange(index.column() == 0 ? 0 : -limit,
                       index.column() == 0 ? 10 : limit);
        spin->setSingleStep(index.column() == 0 ? 0.1 : amplitude_->singleStep());
        return spin;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        static_cast<QDoubleSpinBox *>(editor)->setValue(index.data(Qt::EditRole).toDouble());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        auto spin = static_cast<QDoubleSpinBox *>(editor);
        spin->interpretText();
        if (index.column() == 0) {
            for (int row = 0; row < model->rowCount(); ++row) {
                if (row != index.row() && model->index(row, 0).data(Qt::EditRole).toDouble() == spin->value()) {
                    QToolTip::showText(spin->mapToGlobal(QPoint()), tr("Ce temps existe déjà : chaque point doit avoir un temps distinct."), spin);
                    return;
                }
            }
        }
        model->setData(index, spin->value(), Qt::EditRole);
    }
private:
    QDoubleSpinBox *amplitude_;
};

QDoubleSpinBox *field(const QString &name, double min, double max, double initial, int decimals,
                      double step, const QString &unit) {
    auto spin = new QDoubleSpinBox;
    spin->setObjectName(name);
    spin->setDecimals(decimals);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setValue(initial);
    if (!unit.isEmpty()) spin->setSuffix(" " + unit);
    return spin;
}
}

ConsigneWidget::ConsigneWidget(QWidget *parent) : QGroupBox(tr("Consigne"), parent) {
    setObjectName("consigne"); setProperty("class", "panneauCourbes");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    makeUI();
    qApp->installEventFilter(this);
    connect(type_, &QComboBox::currentIndexChanged, this, &ConsigneWidget::updateForm);
    for (auto spin : {amplitude_, duration_, delay_, rise_, fall_, frequency_, offset_})
        connect(spin, &QDoubleSpinBox::valueChanged, this, &ConsigneWidget::updateLimits);
    connect(invert_, &QCheckBox::toggled, this, [this](bool inverted) {
        const double value = -amplitude_->value();
        {
            const QSignalBlocker blocker(amplitude_);
            amplitude_->setRange(inverted ? -amplitudeMaximum_ : amplitudeMinimum_,
                                 inverted ? -amplitudeMinimum_ : amplitudeMaximum_);
            amplitude_->setValue(value);
        }
        updateTableLimits(true);
        updateLimits();
    });
    updateForm();
}

bool ConsigneWidget::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && table_ && table_->isVisible()) {
        auto target = qobject_cast<QWidget *>(watched);
        // Les boutons d'ajout/suppression utilisent encore la ligne sélectionnée.
        const bool tableAction = target && (target == addPoint_ || target == removePoint_
            || addPoint_->isAncestorOf(target) || removePoint_->isAncestorOf(target));
        const bool emptyArea = target == table_->viewport()
            && !table_->indexAt(static_cast<QMouseEvent *>(event)->position().toPoint()).isValid();
        const bool header = target && (target == table_->horizontalHeader()
            || table_->horizontalHeader()->isAncestorOf(target));
        const bool outside = target && target != table_ && !table_->isAncestorOf(target) && !tableAction;
        if (outside || emptyArea || header) {
            if (auto focus = QApplication::focusWidget(); focus && table_->isAncestorOf(focus))
                focus->clearFocus();
            table_->clearSelection();
            table_->setCurrentItem(nullptr);
            removePoint_->setEnabled(false);
            if (emptyArea) return true; // Empêche la vue de rétablir une ligne courante.
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

void ConsigneWidget::setDebugMode(bool enabled) {
    const auto data = QVariant::fromValue(Protocol::CMD::ConsigneTest);
    const int index = type_->findData(data);
    if (enabled && index < 0) {
        type_->addItem(tr("Test"), data);
    } else if (!enabled && index >= 0) {
        if (type_->currentIndex() == index) type_->setCurrentIndex(0);
        type_->removeItem(index);
    }
}

void ConsigneWidget::makeUI() {
    auto frameLayout = new QVBoxLayout(this);
    const auto margins = frameLayout->contentsMargins();
    frameLayout->setContentsMargins(3, margins.top(), 3, margins.bottom() / 2);
    auto scroll = new QScrollArea;
    scroll->setObjectName("consigneScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frameLayout->addWidget(scroll, 1);
    auto content = new QWidget;
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);

    type_ = new QComboBox;
    type_->setObjectName("typeConsigne");
    type_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    type_->setFocusPolicy(Qt::TabFocus);
    type_->setToolTip(tr("Sélection du type de consigne"));
    type_->setView(new QListView);
    type_->setItemDelegate(new QStyledItemDelegate(type_));
    type_->addItem(tr("Échelon"), QVariant::fromValue(Protocol::CMD::ConsigneEchelon));
    type_->addItem(tr("Rampe"), QVariant::fromValue(Protocol::CMD::ConsigneRampe));
    type_->addItem(tr("Trapèze"), QVariant::fromValue(Protocol::CMD::ConsigneTrapeze));
    type_->addItem(tr("Sinusoïde"), QVariant::fromValue(Protocol::CMD::ConsigneSinus));
    type_->addItem(tr("Tableau"), QVariant::fromValue(Protocol::CMD::ConsigneTableau));
    auto selection = new QHBoxLayout;
    selection->addWidget(type_, 1);
    auto reset = new QPushButton;
    reset->setProperty("class", "boutonIconeConsigne");
    reset->setObjectName("reinitialiserConsigne");
    reset->setIcon(QIcon(":/icons/reset-values.svg"));
    reset->setToolTip(tr("Réinitialiser les valeurs de la consigne"));
    reset->setAccessibleName(reset->toolTip());
    connect(reset, &QPushButton::clicked, this, &ConsigneWidget::resetValues);
    selection->addWidget(reset);
    layout->addLayout(selection);
    type_->ensurePolished();
    reset->setFixedHeight(type_->sizeHint().height());
    layout->addWidget(makeGraphique(), 1);
    layout->addLayout(makeForm());
    testSpace_ = new QWidget;
    testSpace_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    layout->addWidget(testSpace_, 1);
    scroll->setWidget(content); // setWidget()  laisser apparaître le fond du parent.
    content->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setAutoFillBackground(false);
}

QFormLayout *ConsigneWidget::makeForm() {
    invert_ = new QCheckBox;
    invert_->setObjectName("inverserConsigne");
    invert_->setToolTip(tr("Inverser"));
    invert_->setAccessibleName(tr("Inverser"));
    form_ = new QFormLayout;
    amplitude_ = field("amplitude", -255, 255, 255, 0, 1, "");
    duration_ = field("duree", 0.1, 10, 10, 1, 0.1, "s");
    delay_ = field("offset", 0, 10, 0, 1, 0.1, "s");
    auto amplitudeBox = new QWidget;
    amplitudeBox_ = amplitudeBox;
    auto amplitudeLayout = new QHBoxLayout(amplitudeBox);
    amplitudeLayout->setContentsMargins(0, 0, 0, 0);
    amplitudeLayout->addWidget(amplitude_, 1);
    amplitudeLayout->addWidget(invert_);
    amplitudeLabel_ = new QLabel(tr("Amplitude")); form_->addRow(amplitudeLabel_, amplitudeBox);
    form_->addRow(tr("Durée"), duration_);
    delayLabel_ = new QLabel(tr("Délai"));
    form_->addRow(delayLabel_, delay_);
    rise_ = field("dureeMontee", 0, 10, 1, 2, 0.1, "s");
    fall_ = field("dureeDescente", 0, 10, 1, 2, 0.1, "s");
    riseLabel_ = new QLabel(tr("Durée montée")); form_->addRow(riseLabel_, rise_);
    fallLabel_ = new QLabel(tr("Durée descente")); form_->addRow(fallLabel_, fall_);
    frequency_ = field("frequence", 0.01, 100, 1, 2, 0.1, "Hz");
    frequencyLabel_ = new QLabel(tr("Fréquence")); form_->addRow(frequencyLabel_, frequency_);
    offset_ = field("offsetSinus", -255, 255, 0, 0, 1, "");
    offsetLabel_ = new QLabel(tr("Offset"));
    form_->addRow(offsetLabel_, offset_);
    tablePanel_ = makeTableUI();
    form_->addRow(tablePanel_);
    return form_;
}

void ConsigneWidget::resetValues() {
    const QSignalBlocker amplitudeBlock(amplitude_), durationBlock(duration_), delayBlock(delay_),
        riseBlock(rise_), fallBlock(fall_), frequencyBlock(frequency_), offsetBlock(offset_),
        invertBlock(invert_), tableInvertBlock(tableInvert_), tableBlock(table_);
    invert_->setChecked(false);
    tableInvert_->setChecked(false);
    amplitude_->setRange(amplitudeMinimum_, amplitudeMaximum_);
    amplitude_->setValue(std::clamp(255.0, amplitudeMinimum_, amplitudeMaximum_));
    duration_->setValue(10);
    delay_->setMaximum(10);
    delay_->setValue(0);
    rise_->setMaximum(10);
    rise_->setValue(1);
    fall_->setMaximum(10);
    fall_->setValue(1);
    frequency_->setValue(1);
    offset_->setRange(amplitudeMinimum_, amplitudeMaximum_);
    offset_->setValue(0);
    table_->setRowCount(0);
    addTablePoint();
    addTablePoint();
    table_->clearSelection();
    table_->setCurrentItem(nullptr);
    removePoint_->setEnabled(false);
    updateForm();
}

QWidget *ConsigneWidget::makeTableUI() {
    auto panel = new QWidget;
    panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    table_ = new QTableWidget(0, 2);
    // Le tableau s'adapte au panneau existant sans imposer sa largeur préférée.
    table_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    table_->setMinimumWidth(0);
    table_->setObjectName("pointsConsigne");
    table_->setHorizontalHeaderLabels({tr("Temps (s)"), tr("Amplitude")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setItemDelegate(new PointDelegate(amplitude_, table_));
    table_->setMinimumHeight(140);
    layout->addWidget(table_);
    auto buttons = new QHBoxLayout;
    auto add = new QPushButton;
    add->setProperty("class", "boutonIconeConsigne");
    addPoint_ = add;
    add->setIcon(QIcon(":/icons/add-point.svg"));
    add->setObjectName("ajouterPointConsigne");
    add->setToolTip(tr("Ajouter un point"));
    add->setAccessibleName(add->toolTip());
    removePoint_ = new QPushButton;
    removePoint_->setProperty("class", "boutonIconeConsigne");
    removePoint_->setIcon(QIcon(":/icons/remove-point.svg"));
    removePoint_->setObjectName("retirerPointConsigne");
    removePoint_->setToolTip(tr("Retirer le point sélectionné"));
    removePoint_->setAccessibleName(removePoint_->toolTip());
    buttons->addWidget(add);
    buttons->addWidget(removePoint_);
    buttons->addStretch();
    tableInvert_ = new QCheckBox;
    tableInvert_->setText(tr("Inverser"));
    tableInvert_->setLayoutDirection(Qt::RightToLeft);
    tableInvert_->setObjectName("inverserTableau");
    tableInvert_->setToolTip(tr("Inverser le signe de toutes les amplitudes du tableau"));
    tableInvert_->setAccessibleName(tableInvert_->toolTip());
    buttons->addWidget(tableInvert_, 0, Qt::AlignVCenter);
    connect(tableInvert_, &QCheckBox::toggled, invert_, &QCheckBox::setChecked);
    connect(invert_, &QCheckBox::toggled, tableInvert_, &QCheckBox::setChecked);
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, &ConsigneWidget::addTablePoint);
    connect(removePoint_, &QPushButton::clicked, this, [this] {
        if (table_->currentRow() < 0 || table_->rowCount() <= 1) return;
        table_->removeRow(table_->currentRow());
        removePoint_->setEnabled(table_->rowCount() > 1 && table_->currentRow() >= 0);
        updateGraphique();
    });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this] {
        removePoint_->setEnabled(table_->rowCount() > 1 && table_->currentRow() >= 0);
    });
    connect(table_, &QTableWidget::itemChanged, this, [this] {
        table_->sortItems(0, Qt::AscendingOrder);
        updateGraphique();
    });
    // Deux points initiaux utilisant l'amplitude courante.
    addTablePoint();
    addTablePoint();
    return panel;
}

void ConsigneWidget::addTablePoint() {
    const bool appendAtEnd = !table_->selectionModel()->hasSelection();
    const int referenceRow = appendAtEnd ? table_->rowCount() - 1 : table_->currentRow();
    if (referenceRow >= 0
        && table_->item(referenceRow, 0)->data(Qt::EditRole).toDouble() >= 10) {
        QMessageBox::information(this, tr("Ajout d'un point"),
            tr("Impossible d'ajouter un point après 10 s.\n"
               "Sélectionnez une ligne avant 10 s pour insérer un point."));
        return;
    }
    int ticks = referenceRow < 0 ? 0
        : std::min(1000, qRound(table_->item(referenceRow, 0)->data(Qt::EditRole).toDouble() * 100) + 100);
    if (!appendAtEnd && referenceRow >= 0 && referenceRow + 1 < table_->rowCount()) {
        const int left = qRound(table_->item(referenceRow, 0)->data(Qt::EditRole).toDouble() * 100);
        const int right = qRound(table_->item(referenceRow + 1, 0)->data(Qt::EditRole).toDouble() * 100);
        if (right - left < 2) {
            QToolTip::showText(addPoint_->mapToGlobal(QPoint()),
                               tr("Les points sont trop proches pour insérer un temps distinct à 0,01 s près."), addPoint_);
            return;
        }
        ticks = (left + right) / 2;
    }
    auto occupied = [this](int tick) {
        for (int row = 0; row < table_->rowCount(); ++row)
            if (qRound(table_->item(row, 0)->data(Qt::EditRole).toDouble() * 100) == tick) return true;
        return false;
    };
    if (occupied(ticks)) {
        ticks = 0;
        while (ticks <= 1000 && occupied(ticks)) ++ticks;
    }
    if (ticks > 1000) return;
    auto time = new QTableWidgetItem;
    auto value = new QTableWidgetItem;
    time->setData(Qt::EditRole, ticks / 100.0);
    value->setData(Qt::EditRole, amplitude_->value());
    {
        const QSignalBlocker blocker(table_);
        const int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0, time);
        table_->setItem(row, 1, value);
        table_->sortItems(0, Qt::AscendingOrder);
    }
    table_->setCurrentItem(time);
    removePoint_->setEnabled(table_->rowCount() > 1);
    updateGraphique();
}

QVector<QPointF> ConsigneWidget::tablePoints() const {
    QVector<QPointF> points;
    if (!table_) return points;
    for (int row = 0; row < table_->rowCount(); ++row)
        points.append({table_->item(row, 0)->data(Qt::EditRole).toDouble(),
                       table_->item(row, 1)->data(Qt::EditRole).toDouble()});
    return points;
}

void ConsigneWidget::updateTableLimits(bool invertValues) {
    const QSignalBlocker blocker(table_);
    const double limit = std::max(std::abs(amplitudeMinimum_), std::abs(amplitudeMaximum_));
    table_->horizontalHeaderItem(1)->setText(tr("Amplitude") + amplitude_->suffix());
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto item = table_->item(row, 1);
        const double value = item->data(Qt::EditRole).toDouble() * (invertValues ? -1 : 1);
        item->setData(Qt::EditRole, std::clamp(value, -limit, limit));
    }
}

QWidget *ConsigneWidget::makeGraphique() {
    preview_ = new ConsignePreview(this);
    preview_->setMinimumHeight(90);
    return preview_;
}

void ConsigneWidget::updateGraphique() {
    preview_->setSignal(typeConsigne(), {amplitude(), duration_->value(),
        typeConsigne() == Protocol::CMD::ConsigneSinus ? 0 : delay_->value(),
        rise_->value(), fall_->value(), frequency_->value(), offset_->value(), tablePoints()});
}

void ConsigneWidget::updateForm() {
    const bool test = typeConsigne() == Protocol::CMD::ConsigneTest;
    preview_->setVisible(!test);
    testSpace_->setVisible(test);
    const bool tableau = typeConsigne() == Protocol::CMD::ConsigneTableau;
    tablePanel_->setVisible(tableau);
    form_->setRowVisible(amplitudeBox_, !tableau && !test);
    form_->setRowVisible(duration_, !tableau && !test);
    if (tableau || test) {
        for (auto field : {delay_, rise_, fall_, frequency_, offset_}) form_->setRowVisible(field, false);
        updateGraphique();
        return;
    }
    for (auto field : {delay_, rise_, fall_, frequency_, offset_}) form_->setRowVisible(field, true);
    if (typeConsigne() == Protocol::CMD::ConsigneEchelon) {
        amplitudeLabel_->setText(tr("Amplitude"));
        rise_->setVisible(false); riseLabel_->setVisible(false);
        fall_->setVisible(false); fallLabel_->setVisible(false);
        frequency_->setVisible(false); frequencyLabel_->setVisible(false);
        delay_->setVisible(true); delayLabel_->setVisible(true);
        offset_->setVisible(false); offsetLabel_->setVisible(false);
    }
    else if (typeConsigne() == Protocol::CMD::ConsigneRampe) {
        amplitudeLabel_->setText(tr("Valeur finale"));
        rise_->setVisible(false); riseLabel_->setVisible(false);
        fall_->setVisible(false); fallLabel_->setVisible(false);
        frequency_->setVisible(false); frequencyLabel_->setVisible(false);
        delay_->setVisible(true); delayLabel_->setVisible(true);
        offset_->setVisible(false); offsetLabel_->setVisible(false);
    }
    else if (typeConsigne() == Protocol::CMD::ConsigneTrapeze) {
        amplitudeLabel_->setText(tr("Amplitude"));
        rise_->setVisible(true); riseLabel_->setVisible(true);
        fall_->setVisible(true); fallLabel_->setVisible(true);
        frequency_->setVisible(false); frequencyLabel_->setVisible(false);
        delay_->setVisible(true); delayLabel_->setVisible(true);
        offset_->setVisible(false); offsetLabel_->setVisible(false);
    }
    else if (typeConsigne() == Protocol::CMD::ConsigneSinus) {
        amplitudeLabel_->setText(tr("Amplitude"));
        rise_->setVisible(false); riseLabel_->setVisible(false);
        fall_->setVisible(false); fallLabel_->setVisible(false);
        frequency_->setVisible(true); frequencyLabel_->setVisible(true);
        delay_->setVisible(false); delayLabel_->setVisible(false);
        offset_->setVisible(true); offsetLabel_->setVisible(true);
    }
    else {
        return;
    }
    updateLimits();
}

void ConsigneWidget::updateLimits() {
    const QSignalBlocker offsetBlock(delay_), riseBlock(rise_), fallBlock(fall_);
    delay_->setMaximum(duration_->value());
    const double available = duration_->value() - delay_->value();
    rise_->setMaximum(available); fall_->setMaximum(available);
    // Keep the generated plateau nonnegative, including when the total duration shrinks.
    const double sum = rise_->value() + fall_->value();
    if (sum > available && sum > 0) {
        rise_->setValue(std::floor(100 * available * rise_->value() / sum) / 100);
        fall_->setValue(std::floor(100 * (available - rise_->value())) / 100);
    }
    if (typeConsigne() == Protocol::CMD::ConsigneSinus) {
        const QSignalBlocker amplitudeBlock(amplitude_), sinusOffsetBlock(offset_);
        const double radius = (amplitude_->maximum() - amplitude_->minimum()) / 2;
        amplitude_->setValue(std::clamp(amplitude_->value(), -radius, radius));
        const double peak = std::abs(amplitude());
        // Garder l'amplitude demandée et déplacer l'offset si une crête atteint la limite.
        offset_->setRange(amplitudeMinimum_ + peak, amplitudeMaximum_ - peak);
    }
    updateGraphique();
}

void ConsigneWidget::setLimitesAmplitude(double minimum, double maximum, const QString &unit) {
    const QSignalBlocker blocker(amplitude_);
    amplitudeMinimum_ = minimum;
    amplitudeMaximum_ = maximum;
    amplitude_->setRange(invert_->isChecked() ? -maximum : minimum,
                         invert_->isChecked() ? -minimum : maximum);
    amplitude_->setSuffix(unit.isEmpty() ? QString() : " " + unit);
    const QSignalBlocker sinusOffsetBlock(offset_);
    offset_->setRange(minimum, maximum);
    offset_->setSuffix(amplitude_->suffix());
    updateTableLimits();
    updateLimits();
}

double ConsigneWidget::amplitude() const { return amplitude_->value(); }

Protocol::CMD ConsigneWidget::typeConsigne() const {
    return type_->currentData().value<Protocol::CMD>();
}

QVector<Protocol::CommandRequest> ConsigneWidget::getCommand() const {
    auto number = [](double value) { return QString::number(value, 'f', 2); };
    auto milliseconds = [&number](double seconds) { return number(seconds * 1000); };
    const auto type = typeConsigne();
    if (type == Protocol::CMD::ConsigneTest) return {{type, {}}};
    if (type == Protocol::CMD::ConsigneTableau) {
        const auto points = tablePoints();
        QVector<Protocol::CommandRequest> commands{{type, {QString::number(points.size())}}};
        for (const auto &point : points)
            commands.append({type, {milliseconds(point.x()), number(point.y())}});
        return commands;
    }
    const auto duration = milliseconds(duration_->value() - (type == Protocol::CMD::ConsigneSinus ? 0 : delay_->value()));
    const auto offset = milliseconds(delay_->value());
    const auto value = number(amplitude());
    QStringList values;
    if (type == Protocol::CMD::ConsigneEchelon) {
        values = {value, duration, offset};
    }
    else if (type == Protocol::CMD::ConsigneRampe) {
        values = {number(0), value, duration, offset};
    }
    else if (type == Protocol::CMD::ConsigneTrapeze) {
        values = {value, milliseconds(rise_->value()),
                  milliseconds(std::max(0.0, duration_->value() - delay_->value() - rise_->value() - fall_->value())),
                  milliseconds(fall_->value()), offset};
    }
    else if (type == Protocol::CMD::ConsigneSinus) {
        values = {value, milliseconds(1 / frequency_->value()), duration, number(offset_->value())};
    }
    else {
        return {};
    }
    return {{type, values}};
}

QString ConsigneWidget::description() const {
    if (typeConsigne() == Protocol::CMD::ConsigneTest) return tr("Test");
    auto number = [](double value) {
        QLocale locale;
        locale.setNumberOptions(QLocale::OmitGroupSeparator);
        return locale.toString(value, 'f', 2);
    };
    const auto type = typeConsigne();
    if (type == Protocol::CMD::ConsigneTableau) {
        QStringList points;
        for (const auto &point : tablePoints())
            points.append(QString("(%1 s ; %2%3)").arg(number(point.x()), number(point.y()), amplitude_->suffix()));
        return tr("Tableau (interpolation linéaire) : ") + points.join(", ");
    }
    QString name;
    QStringList parameters{QString("Amplitude %1").arg(number(amplitude()))};
    parameters << QStringLiteral("Durée %1 [s]").arg(number(duration_->value() - (type == Protocol::CMD::ConsigneSinus ? 0 : delay_->value())));
    if (type != Protocol::CMD::ConsigneSinus)
        parameters << QStringLiteral("Délai %1 [s]").arg(number(delay_->value()));
    if (type == Protocol::CMD::ConsigneEchelon) {
        name = QStringLiteral("Echelon");
    }
    else if (type == Protocol::CMD::ConsigneRampe) {
        name = QStringLiteral("Rampe");
        parameters << QStringLiteral("Départ %1").arg(number(0));
    }
    else if (type == Protocol::CMD::ConsigneTrapeze) {
        name = QStringLiteral("Trapèze");
        parameters << QStringLiteral("Montée %1 [s]").arg(number(rise_->value()))
                   << QStringLiteral("Plateau %1 [s]").arg(number(std::max(0.0,
                          duration_->value() - delay_->value() - rise_->value() - fall_->value())))
                   << QStringLiteral("Descente %1 [s]").arg(number(fall_->value()));
    }
    else if (type == Protocol::CMD::ConsigneSinus) {
        name = QStringLiteral("Sinusoïde");
        parameters << QStringLiteral("Offset %1%2").arg(number(offset_->value()), offset_->suffix());
        parameters << QStringLiteral("Fréquence %1 [Hz]").arg(number(frequency_->value()));
    }
    else {
        return {};
    }
    return name + " : " + parameters.join(", ");
}


/* ConsignePreview */
ConsignePreview::ConsignePreview(ConsigneWidget *source) : QWidget(source), source_(source) {
    setObjectName("apercuConsigne");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Aperçu de la consigne"));
}

QSize ConsignePreview::sizeHint() const { return QSize(240, 150); }
QSize ConsignePreview::minimumSizeHint() const { return QSize(0, 0); }

void ConsignePreview::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), source_->previewBackgroundColor_);
    const QRectF plot = QRectF(rect()).adjusted(12, 10, -10, -10);
    if (plot.width() <= 0 || plot.height() <= 0) return;
    // Réserver 5 % de chaque côté, au minimum la demi-épaisseur du trait anticrénelé.
    const double strokeMargin = source_->previewCurveWidth_ / 2 + 0.5;
    const double marginX = std::max(plot.width() * 0.05, strokeMargin);
    const double marginY = std::max(plot.height() * 0.05, strokeMargin);
    const QRectF dataArea = plot.adjusted(marginX, marginY, -marginX, -marginY);
    if (dataArea.width() <= 0 || dataArea.height() <= 0) return;
    // Échelle fixe : 100 % correspond à la plus grande amplitude autorisée en valeur absolue.
    const double maximum = std::max(std::abs(source_->amplitudeMinimum_),
                                    std::abs(source_->amplitudeMaximum_));
    const double reference = maximum > 0 ? maximum : 1;
    bool hasNegative = false, hasPositive = false;
    if (type_ == Protocol::CMD::ConsigneTableau) {
        for (const auto &sample : parameters_.points) {
            hasNegative |= sample.y() < 0;
            hasPositive |= sample.y() > 0;
        }
    } else {
        for (int i = 0; i <= 500; ++i) {
            const double value = valeur(11.0 * i / 500);
            hasNegative |= value < 0;
            hasPositive |= value > 0;
        }
    }
    const double low = hasNegative ? -100 : 0;
    const double high = hasPositive || !hasNegative ? 100 : 0;
    auto point = [&](double t, double y) {
        return QPointF(dataArea.left() + dataArea.width() * t / 11,
                       dataArea.bottom() - dataArea.height() * (100 * y / reference - low) / (high - low));
    };
    QPainterPath path; path.moveTo(point(0, 0));
    for (int i = 0; i <= 500; ++i) {
        double t = 11.0 * i / 500;
        path.lineTo(point(t, valeur(t)));
    }
    if (type_ == Protocol::CMD::ConsigneTableau && !parameters_.points.isEmpty()) {
        path = QPainterPath(point(0, parameters_.points.first().y()));
        for (const auto &sample : parameters_.points) path.lineTo(point(sample.x(), sample.y()));
        path.lineTo(point(11, parameters_.points.last().y()));
    }
    painter.setClipRect(plot);
    const auto origin = point(0, 0);
    painter.setPen(QPen(source_->previewZeroColor_, source_->previewZeroWidth_));
    painter.drawLine(QPointF(plot.left(), origin.y()), QPointF(plot.right(), origin.y()));
    painter.setPen(QPen(source_->previewCurveColor_, source_->previewCurveWidth_)); painter.drawPath(path);
    // Conserver l'axe vertical au premier plan.
    painter.setPen(QPen(source_->previewZeroColor_, source_->previewZeroWidth_));
    painter.drawLine(QPointF(origin.x(), plot.bottom()), QPointF(origin.x(), plot.top()));
}

void ConsignePreview::setSignal(Protocol::CMD type, const Parameters &parameters) {
    type_ = type;
    parameters_ = parameters;
    update();
}

double ConsignePreview::valeur(double time) const {
    const auto &p = parameters_;
    if (type_ == Protocol::CMD::ConsigneTableau) {
        if (p.points.isEmpty()) return 0;
        if (p.points.size() == 1 || time <= p.points.first().x()) return p.points.first().y();
        if (time >= p.points.last().x()) return p.points.last().y();
        for (qsizetype i = 1; i < p.points.size(); ++i) {
            const auto &left = p.points[i - 1];
            const auto &right = p.points[i];
            if (time < right.x())
                return left.y() + (right.y() - left.y()) * (time - left.x()) / (right.x() - left.x());
        }
        return 0;
    }
    if (time < p.offset) return 0;
    if (type_ == Protocol::CMD::ConsigneEchelon) {
        return time < p.duration ? p.amplitude : 0;
    }
    else if (type_ == Protocol::CMD::ConsigneRampe) {
        if (time <= p.offset) return 0;
        return time >= p.duration ? p.amplitude
            : p.amplitude * (time - p.offset) / (p.duration - p.offset);
    }
    else if (type_ == Protocol::CMD::ConsigneTrapeze) {
        if (time >= p.duration) return 0;
        if (time - p.offset < p.rise) return p.amplitude * (time - p.offset) / p.rise;
        if (p.duration - time < p.fall) return p.amplitude * (p.duration - time) / p.fall;
        return p.amplitude;
    }
    else if (type_ == Protocol::CMD::ConsigneSinus) {
        return p.sinusOffset + p.amplitude * std::sin(2 * 3.14159265358979323846
            * p.frequency * (std::min(time, p.duration) - p.offset));
    }
    else {
        return 0;
    }
}
