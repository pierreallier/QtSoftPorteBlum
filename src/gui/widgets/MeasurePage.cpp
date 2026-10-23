#include "gui/components/TwoStateButton.h"
#include "gui/components/CalibrationButton.h"
#include "serial/Protocol.h"
#include "gui/widgets/MeasurePage.h"
#include "gui/widgets/StateGraphWidget.h"
#include "gui/widgets/GraphWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QFrame>
#include <QSignalBlocker>

MeasurePage::MeasurePage(QWidget *parent) : Page(parent) {
    setLayout(makeUI());
    setSystemState(systemState());

    connect(calibrate_, &CalibrationButton::sendCommand, this, &Page::sendCommand);
    connect(graph_, &StateGraphWidget::sendRawCommand, this, &Page::sendRawCommand);
    connect(curves_, &GraphWidget::accelerometerToggled, this, &MeasurePage::toggleAccelerometer);
    connect(acquire_, &QPushButton::toggled, this, &MeasurePage::toggleAcquisition);
}

void MeasurePage::triggerPrimaryAction() {
    acquire_->click();
}

void MeasurePage::setVisible(bool visible) {
    calibrate_->setShortcut(visible ? QKeySequence("Ctrl+C") : QKeySequence());
    curves_->setVisible(visible);
    Page::setVisible(visible);
}

QHBoxLayout* MeasurePage::makeUI() {
    auto layout = new QHBoxLayout;
    
    // Left panel with controls
    controlsPanel_ = makeControlsUI();
    layout->addWidget(controlsPanel_);

    auto separator = new QFrame;
    separator->setProperty("class", "separateurVertical");
    auto separatorLayout = new QVBoxLayout;
    separatorLayout->setContentsMargins(0, 8, 0, 8);
    separatorLayout->addWidget(separator);
    layout->addLayout(separatorLayout);

    // Right panel with curves
    auto right = new QVBoxLayout;
    curves_ = new GraphWidget;
    curves_->setConsigneUnit("PWM");
    right->addWidget(curves_, 1);
    layout->addLayout(right, 1);
    return layout;
}

QWidget* MeasurePage::makeControlsUI() {
    auto left = new QWidget;
    auto controls = new QVBoxLayout(left);
    // Bouton Acquérir / Arrêter
    auto buttons = new QGridLayout;
    acquire_ = new TwoStateButton;
    acquire_->setObjectName("boutonRunStop");
    acquire_->setStateText(false, tr("Acqu\u00e9rir"));
    acquire_->setStateIcon(false, QIcon(":/icons/play.svg"));
    acquire_->setStateToolTip(false, tr("Démarrer l'acquisition de mesures"));
    acquire_->setStateText(true, tr("Arr\u00eater"));
    acquire_->setStateIcon(true, QIcon(":/icons/stop.svg"));
    acquire_->setStateToolTip(true, tr("Arrêter l'acquisition de mesures"));
    acquire_->setMaximumWidth(QWIDGETSIZE_MAX);
    acquire_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Bouton Calibration
    calibrate_ = new CalibrationButton(this);
    calibrate_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttons->addWidget(acquire_, 0, 0);
    buttons->addWidget(calibrate_, 0, 1);
    buttons->setColumnStretch(0, 1);
    buttons->setColumnStretch(1, 1);
    controls->addLayout(buttons);
    // Graphe d'état
    graph_ = new StateGraphWidget;
    controls->addWidget(graph_, 1);
    left->setFixedWidth(controls->sizeHint().width());
    return left;
}

void MeasurePage::setDebugMode(bool enabled) {
    graph_->setDebugMode(enabled);
    curves_->setDebugMode(enabled);
}

void MeasurePage::toggleAcquisition(bool on) {
    if (on) curves_->reset();
    emit sendCommand(on ? Protocol::CMD::MesuresOn : Protocol::CMD::MesuresOff);
}

void MeasurePage::toggleAccelerometer(bool on) {
    curves_->setAccelerometerControlEnabled(false);
    emit sendCommand(on ? Protocol::CMD::AccelerometreOn : Protocol::CMD::AccelerometreOff);
}

void MeasurePage::setSystemState(const SystemState &state) {
    const auto previous = systemState();
    Page::setSystemState(state);

    if (previous.connected != state.connected || (previous.initialized && !state.initialized))
        graph_->reset();
    const bool ready = state.canSendCommands();
    graph_->setEnabled(ready);

    curves_->setAccelerometerEnabled(state.accelerometer);
    curves_->setAccelerometerControlEnabled(ready);

    acquire_->setEnabled(ready && !state.calibration);
    calibrate_->setEnabled(ready && !state.calibration);

    const QSignalBlocker blocker(acquire_);
    acquire_->setChecked(state.measuring);
}

void MeasurePage::commandFinished(Protocol::CMD command, bool) {
    if (command == Protocol::CMD::AccelerometreOn || command == Protocol::CMD::AccelerometreOff)
        setSystemState(systemState());
}

void MeasurePage::processEvents(const EventQueue::Batch &batch) {
    for (const auto &entry : batch.entries) {
        if (const auto *event = dynamic_cast<const StateEvent *>(entry.event.get()))
            graph_->setState(event->value);
    }
}

void MeasurePage::processMeasurements(const EventQueue::Batch &batch) {
    curves_->append(batch);
}

void MeasurePage::requestExit() {
    if (systemState().measuring) emit sendCommand(Protocol::CMD::MesuresOff);
    emit readyToExit(true);
}
