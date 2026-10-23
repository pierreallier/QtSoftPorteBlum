#include "gui/components/TwoStateButton.h"
#include "gui/components/CalibrationButton.h"
#include "serial/Protocol.h"
#include "gui/widgets/PilotagePage.h"
#include "gui/widgets/PIDWidget.h"
#include "gui/widgets/ConsigneWidget.h"
#include "gui/widgets/GraphWidget.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <QSignalBlocker>
#include <QProgressDialog>
#include <QProgressBar>
#include <QLabel>
#include <QMessageBox>
#include <QShortcut>
#include <limits>

PilotagePage::PilotagePage(QWidget *parent) : Page(parent) {
    setObjectName("pilotage");
    setLayout(makeUI());

    acquisitionTimer_.setSingleShot(true);
    acquisitionTimer_.setTimerType(Qt::PreciseTimer);
    connect(&acquisitionTimer_, &QTimer::timeout, this, &PilotagePage::stopPilotage);


    connect(send_, &QPushButton::toggled, this, &PilotagePage::togglePilotage);
    connect(calibrate_, &CalibrationButton::sendCommand, this, &Page::sendCommand);
    connect(asservissement_, &QGroupBox::toggled, this, &PilotagePage::updateSelection);
    connect(speed_, &PIDWidget::toggled, this, &PilotagePage::speedSelectionChanged);
    connect(position_, &PIDWidget::toggled, this, &PilotagePage::positionSelectionChanged);
    connect(curves_, &GraphWidget::accelerometerToggled, this, &PilotagePage::toggleAccelerometer);
    updateSelection();
    setSystemState(systemState());
}

void PilotagePage::triggerPrimaryAction() {
    send_->click();
}

void PilotagePage::setVisible(bool visible) {
    calibrate_->setShortcut(visible ? QKeySequence("Ctrl+C") : QKeySequence());
    curves_->setVisible(visible);
    Page::setVisible(visible);
}

QHBoxLayout* PilotagePage::makeUI() {
    auto layout = new QHBoxLayout;

    controlsPanel_ = makeControlsUI();
    layout->addWidget(controlsPanel_);

    auto separator = new QFrame;
    separator->setProperty("class", "separateurVertical");
    auto separatorLayout = new QVBoxLayout;
    separatorLayout->setContentsMargins(0, 8, 0, 8);
    separatorLayout->addWidget(separator);
    layout->addLayout(separatorLayout);

    curves_ = new GraphWidget;
    curves_->setConsigneUnit("PWM");
    layout->addWidget(curves_, 1);
    return layout;
}

QWidget* PilotagePage::makeControlsUI() {
    auto left = new QWidget;
    left->setObjectName("pilotageControls");

    auto controls = new QVBoxLayout(left);
    controls->addWidget(makeAsservissementUI());

    consigne_ = new ConsigneWidget;
    controls->addWidget(consigne_, 1);

    auto actions = new QHBoxLayout;
    send_ = new TwoStateButton;
    send_->setObjectName("boutonRunStop");
    send_->setStateText(false, tr("Envoyer"));
    send_->setStateIcon(false, QIcon(":/icons/play.svg"));
    send_->setStateToolTip(false,tr("Envoyer la consigne au système"));
    send_->setStateText(true, tr("Stopper"));
    send_->setStateIcon(true, QIcon(":/icons/stop.svg"));
    send_->setStateToolTip(true,tr("Stopper le pilotage"));
    send_->setMaximumWidth(QWIDGETSIZE_MAX);
    send_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    calibrate_ = new CalibrationButton(this);
    calibrate_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actions->addWidget(send_, 1);
    actions->addWidget(calibrate_, 1);

    controls->addLayout(actions);
    left->ensurePolished();
    left->setFixedWidth(left->sizeHint().width());
    return left;
}

QGroupBox* PilotagePage::makeAsservissementUI() {
    // Création du panneau de sélection des PID, avec un séparateur vertical.
    asservissement_ = new QGroupBox(tr("Asservissement"));
    asservissement_->setObjectName("asservissement");
    asservissement_->setProperty("class", "panneauCourbes");
    asservissement_->setCheckable(true);
    auto pidLayout = new QHBoxLayout(asservissement_);
    pidLayout->setContentsMargins(0, 0, 0, 0);
    // PID de Vitesse
    speed_ = new PIDWidget(tr("Vitesse"));
    speed_->setObjectName("pidVitesse");
    // PID de position
    position_ = new PIDWidget(tr("Position"));
    position_->setObjectName("pidPosition");
    // Séparateur vertical entre les deux PID
    auto separator = new QFrame;
    separator->setProperty("class", "separateurVertical");
    auto separatorLayout = new QVBoxLayout;
    separatorLayout->setContentsMargins(0, 8, 0, 8);
    separatorLayout->addWidget(separator);
    // Ajout des PID et du séparateur au layout
    pidLayout->addWidget(speed_);
    pidLayout->addLayout(separatorLayout);
    pidLayout->addWidget(position_);
    asservissement_->setChecked(false);
    return asservissement_;
}

void PilotagePage::makeProgressDialog() {
    progress_ = new QProgressDialog(this);
    progress_->setObjectName("pilotageProgress");
    progress_->setWindowTitle(tr("Envoi de la consigne"));

    auto message = new QLabel(progress_);
    message->setObjectName("pilotageProgressMessage");
    message->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    progress_->setLabel(message);

    auto bar = new QProgressBar(progress_);
    bar->setTextVisible(true);
    bar->setFormat("%p%");
    bar->setAlignment(Qt::AlignCenter);
    
    progress_->setBar(bar);
    progress_->setCancelButtonText(tr("Annuler"));
    progress_->setWindowModality(Qt::WindowModal);
    // The modal progress window owns keyboard focus while commands are sent.
    auto stopShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+P")), progress_);
    stopShortcut->setAutoRepeat(false);
    connect(stopShortcut, &QShortcut::activated, this, &PilotagePage::stopPilotage);
    // Visibility is controlled by the send button, never by the internal timer.
    progress_->setMinimumDuration(std::numeric_limits<int>::max());
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    progress_->hide();
    connect(progress_, &QProgressDialog::canceled, this, &PilotagePage::stopPilotage);
}

void PilotagePage::togglePilotage(bool on) {
    const QSignalBlocker blocker(send_);
    send_->setChecked(running_);
    if (!on) { stopPilotage(); return; }
    if (!progress_) makeProgressDialog();
    commands_ = commandRequests();
    completedCommands_ = 0;
    progress_->setRange(0, commands_.size());
    progress_->setValue(0);
    setRunning(true);
    progress_->show();
    if (running_) sendNextCommand();
}

void PilotagePage::updateProgress(int completed, const QString &command) {
    if (!running_) return;
    progress_->setValue(completed);
    if (running_) progress_->setLabelText(tr("Envoi : %1").arg(command));
}

void PilotagePage::sendNextCommand() {
    if (!running_ || acquiring_ || pending_) return;
    if (commands_.isEmpty()) {
        progress_->setValue(progress_->maximum());
        if (!running_) return;
        progress_->hide();
        startAcquisition();
        return;
    }
    const auto request = commands_.takeFirst();
    pending_ = request;
    updateProgress(completedCommands_, request.text());
    if (!running_ || !pending_) return;

    emit sendCommand(request.command, request.values, true);
}

void PilotagePage::failCommand() {
    const auto command = pending_->text();
    stopPilotage();
    QMessageBox dialog(QMessageBox::Critical, tr("Échec du pilotage"),
        tr("Échec du pilotage"),
        QMessageBox::Ok, this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setInformativeText(tr("La commande « %1 » a échoué. L'envoi a été annulé.").arg(command));
    dialog.exec();
}

void PilotagePage::setDebugMode(bool enabled) {
    consigne_->setDebugMode(enabled);
    curves_->setDebugMode(enabled);
}

void PilotagePage::toggleAccelerometer(bool on) {
    curves_->setAccelerometerControlEnabled(false);
    emit sendCommand(on ? Protocol::CMD::AccelerometreOn : Protocol::CMD::AccelerometreOff);
}

void PilotagePage::speedSelectionChanged() { updatePidSelection(speed_); }
void PilotagePage::positionSelectionChanged() { updatePidSelection(position_); }
void PilotagePage::updateSelection() { updatePidSelection(nullptr); }

void PilotagePage::setSystemState(const SystemState &state) {
    const auto previous = systemState();
    Page::setSystemState(state);
    if (previous.connected != state.connected
        || (previous.initialized && !state.initialized)) {
        finishRun();
        recentMeasurements_ = {};
    }
    curves_->setAccelerometerEnabled(state.accelerometer);
    curves_->setAccelerometerControlEnabled(state.canSendCommands());
    updateButtons();
}

void PilotagePage::commandFinished(Protocol::CMD command, bool accepted) {
    if (!running_ || !pending_
        || pending_->command != command) return;
    if (!accepted) { failCommand(); return; }
    pending_.reset();
    ++completedCommands_;
    sendNextCommand();
}

void PilotagePage::processMeasurements(const EventQueue::Batch &batch) {
    for (const auto &entry : batch.entries)
        if (entry.event->kind() == Event::Kind::Measure)
            recentMeasurements_.entries = {entry};
    if (acquiring_) curves_->append(batch);
}

void PilotagePage::updatePidSelection(PIDWidget *changed) {

    // Forcer la sélection d'au moins un PID si l'asservissement est activé.
    if (asservissement_->isChecked() && !speed_->isChecked() && !position_->isChecked()) {
        auto fallback = changed == speed_ ? position_ : speed_;
        const QSignalBlocker blocker(fallback);
        fallback->setChecked(true);
    }
    // Ajuster les limites de la consigne et l'affichage dans le Graphique
    const auto type = typeAsservissement();
    if (type == TypeAsservissement::Aucun) {
        consigne_->setLimitesAmplitude(-255, 255, "");
    } else if (type == TypeAsservissement::Vitesse) {
        consigne_->setLimitesAmplitude(-10, 10, "rad/s");
    } else {
        consigne_->setLimitesAmplitude(-150, 150, QString::fromUtf8("°"));
    }
}

PilotagePage::TypeAsservissement PilotagePage::typeAsservissement() const {
    if (!asservissement_->isChecked()) return TypeAsservissement::Aucun;
    if (speed_->isChecked() && position_->isChecked()) return TypeAsservissement::PositionVitesse;
    return position_->isChecked() ? TypeAsservissement::Position : TypeAsservissement::Vitesse;
}

QVector<Protocol::CommandRequest> PilotagePage::commandRequests() const {
    QVector<Protocol::CommandRequest> commands;
    switch (typeAsservissement()) {
    case TypeAsservissement::Aucun: 
        commands.append({Protocol::CMD::ModePWM, {}});
        break;
    case TypeAsservissement::Vitesse: 
        commands.append({Protocol::CMD::ModeVitesse, {}});
        commands += speed_->getCommand(Protocol::CMD::SetPidVitesse);
        break;
    case TypeAsservissement::Position: 
        commands.append({Protocol::CMD::ModePosition, {}});
        commands += position_->getCommand(Protocol::CMD::SetPidPosition);
        break;
    case TypeAsservissement::PositionVitesse: 
        commands.append({Protocol::CMD::ModePV, {}});
        commands += speed_->getCommand(Protocol::CMD::SetPidVitesse);
        commands += position_->getCommand(Protocol::CMD::SetPidPosition);
        break;
    }
    commands += consigne_->getCommand();
    commands.append({Protocol::CMD::MesuresOn, {}});
    commands.append({Protocol::CMD::Pilotage, {}});
    return commands;
}

void PilotagePage::setRunning(bool running) {
    running_ = running;
    const QSignalBlocker blocker(send_);
    send_->setChecked(running);
    asservissement_->setEnabled(!running);
    consigne_->setEnabled(!running);
    updateButtons();
}

void PilotagePage::updateButtons() {
    const bool ready = systemState().canSendCommands() && !systemState().calibration;
    send_->setEnabled(ready);
    calibrate_->setEnabled(ready && !running_);
}

QString PilotagePage::modeDescription() const {
    QString mode;
    switch (typeAsservissement()) {
    case TypeAsservissement::Aucun:
        mode = QStringLiteral("Boucle Ouverte - PWM"); break;
    case TypeAsservissement::Vitesse:
        mode = QStringLiteral("Boucle Fermée - Vitesse ") + speed_->description(); break;
    case TypeAsservissement::Position:
        mode = QStringLiteral("Boucle Fermée - Position ") + position_->description(); break;
    case TypeAsservissement::PositionVitesse:
        mode = QStringLiteral("Boucle Fermée - Vitesse ") + speed_->description()
            + QStringLiteral(" puis Position ") + position_->description(); break;
    }
    return mode;
}

void PilotagePage::startAcquisition() {
    curves_->reset();
    const auto type = typeAsservissement();
    curves_->setConsigneUnit(type == TypeAsservissement::Aucun ? QString("PWM")
        : type == TypeAsservissement::Vitesse ? QString("rad/s") : QString::fromUtf8("°"));
    const auto mode = modeDescription();
    curves_->setRecordingMetadata({mode, "CONSIGNE " + consigne_->description()});
    acquiring_ = true;
    curves_->append(recentMeasurements_);
    // Acquisition des nouvelles mesures uniquement après toutes les confirmations.
    acquisitionTimer_.start(10000);
}

void PilotagePage::stopPilotage() {
    if (running_) emit sendCommand(Protocol::CMD::Stop);
    finishRun();
}

void PilotagePage::requestExit() {
    stopPilotage();
}

void PilotagePage::finishRun() {
    acquisitionTimer_.stop();
    commands_.clear();
    pending_.reset();
    if (progress_) progress_->hide();
    const bool stopAcquisition = acquiring_ || systemState().measuring;
    acquiring_ = false;
    if (stopAcquisition) emit sendCommand(Protocol::CMD::MesuresOff);
    setRunning(false);
    emit readyToExit(true);
}
