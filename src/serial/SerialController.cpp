#include "serial/SerialController.h"
#include "serial/Protocol.h"
#include "core/AppSettings.h"
#include <algorithm>
SerialController::SerialController(QObject *parent, const QString &settingsPath)
    : QObject(parent), settings_(AppSettings::instance(settingsPath)) {
    qRegisterMetaType<SystemState>();
    connect(this, &SerialController::stateReceived, this, [this](int value) {
        // INIT and confirmed Production states clear the board status messages.
        if (value == settings_.stateId(QStringLiteral("PROD_INIT")) ||
            value == settings_.stateId(QStringLiteral("PROD_OUVERTURE")) ||
            value == settings_.stateId(QStringLiteral("PROD_FERMETURE")) ||
            value == settings_.stateId(QStringLiteral("PROD_PILOTAGE")) ||
            value == settings_.stateId(QStringLiteral("PROD_CALIBRATION"))) {
            emit resetErrors();
        }
        auto state = state_;
        if (value == settings_.stateId(QStringLiteral("PROD_CALIBRATION"))) state.calibration = true;
        else state.calibration = false;
        setSystemState(state);
    });
    automatic = settings_.value("serial/automatic").toBool();
    manualPort = settings_.value("serial/port").toString();
    baud = (automatic ? AppSettings::defaultValue("serial/baud") : settings_.value("serial/baud")).toInt();
    connect(&scanTimer_, &QTimer::timeout, this, &SerialController::scan);
    scanTimer_.start(1500); scan();
}
SerialController::~SerialController() { shutdown(); }
void SerialController::setSystemState(SystemState state) {
    if (!state.connected) state.initialized = false;
    if (!state.initialized) state.measuring = state.accelerometer = state.calibration = false;
    if (state == state_) return;
    const auto previous = state_;
    state_ = state;
    emit systemStateChanged(state);
    // Compatibility signals are derived from the same snapshot.
    if (previous.connected != state.connected) emit connectionChanged(state.connected, state.port);
    if (!previous.initialized && state.initialized) emit commandsReady();
    if (previous.measuring != state.measuring) emit acquisitionRequested(state.measuring);
    if (previous.accelerometer != state.accelerometer) emit accelerometerChanged(state.accelerometer);
}

void SerialController::updateDeviceStatus(char type, quint8 code, qint32 value) {
    if (!state_.canSendCommands()) return;
    auto state = state_;
    if (code == settings_.codeId("ERR_ACCEL_ABSENT") && (type == 'E' || type == 'N')) {
        state.accelerometer = false;
    } else if (type == 'O' || type == 'I' || type == 'R') {
        if (code == settings_.codeId("INFO_ACCEL_SET") || code == settings_.codeId("INFO_ACCEL_GET"))
            state.accelerometer = value != 0;
        else if (code == settings_.codeId("INFO_MESURES_DESACTIVE")) state.measuring = false;
        else if (code == settings_.codeId("DATA_FREQ_MESURES")) state.measuring = value > 0;
        else return;
    } else return;
    setSystemState(state);
}
bool SerialController::saveSettings(QString *error) {
    if (automatic) baud = AppSettings::defaultValue("serial/baud").toInt();
    if (!settings_.save({{"serial/automatic", automatic}, {"serial/port", manualPort},
                            {"serial/baud", baud}}, error)) return false;
    scan();
    return true;
}
bool SerialController::applySerialSettings(bool automaticMode, const QString &portName, qint32 baudRate, QString *error) {
    const QString port = portName.trimmed();
    const qint32 rate = automaticMode ? AppSettings::defaultValue("serial/baud").toInt() : baudRate;
    if (rate <= 0 || (!automaticMode && port.isEmpty())) {
        if (error) *error = tr("Paramètres série invalides.");
        return false;
    }
    // Une erreur de sauvegarde ne doit modifier ni la configuration ni la connexion.
    if (!settings_.save({{"serial/automatic", automaticMode}, {"serial/port", port},
                         {"serial/baud", rate}}, error)) return false;
    const bool changed = automatic != automaticMode || baud != rate
                      || (!automaticMode && manualPort != port);
    if (changed) disconnectPort(); // Annule aussi une connexion/reconnexion en cours.
    automatic = automaticMode;
    manualPort = port;
    baud = rate;
    scan();
    return true;
}

QString SerialController::selectedPort() const {
    if (thread_) return activePort_;
    if (!automatic) return manualPort;
    return ports_.isEmpty() ? QString{} : ports_.first().portName();
}
void SerialController::scan() {
    if (thread_) return;
    ports_ = QSerialPortInfo::availablePorts();
    const auto priority = [](const QSerialPortInfo &p) {
        if (p.manufacturer().contains("Arduino", Qt::CaseInsensitive) ||
            p.description().contains("Arduino", Qt::CaseInsensitive)) return 0;
        if (p.hasVendorIdentifier() && (p.vendorIdentifier() == 0x2341 || p.vendorIdentifier() == 0x2a03)) return 0;
        return 1;
    };
    std::sort(ports_.begin(), ports_.end(), [&](const auto &a, const auto &b) {
        if (priority(a) != priority(b)) return priority(a) < priority(b);
        return a.portName() < b.portName();
    });
    auto state = state_;
    state.port = selectedPort(); state.baud = baud;
    setSystemState(state);
    emit portsChanged();
    if (connectionWanted_ && automatic && !ports_.isEmpty()) connectPort();
}
void SerialController::connectPort() {
    if (thread_) return;
    if (automatic) baud = AppSettings::defaultValue("serial/baud").toInt();
    connectionWanted_ = true;
    activePort_ = selectedPort();
    if (activePort_.isEmpty()) { emit fault("Aucun port disponible"); return; }
    thread_ = new QThread(this);
    auto worker = new SerialWorker(queues_);
    auto state = state_;
    state.port = activePort_; state.baud = baud; state.initialized = false;
    setSystemState(state);
    startupError_.clear();
    lastSerialError_.clear();
    worker->moveToThread(thread_);
    const auto name = activePort_; const auto rate = baud;
    connect(thread_, &QThread::started, worker, [worker, name, rate] { worker->open(name, rate); });
    connect(this, &SerialController::sendRequested, worker, &SerialWorker::send, Qt::QueuedConnection);
    connect(this, &SerialController::stopRequested, worker, &SerialWorker::stop, Qt::QueuedConnection);
    connect(worker, &SerialWorker::opened, this, [this] {
        auto state = state_; state.connected = true; setSystemState(state);
    });
    connect(worker, &SerialWorker::fault, this, [this](const QString &text) {
        lastSerialError_ = text;
        emit fault(text);
    });
    connect(worker, &SerialWorker::resetDetected, this, [this] {
        auto state = state_;
        state.initialized = state.measuring = state.accelerometer = state.calibration = false;
        setSystemState(state);
        emit resetDetected();
    });
    connect(worker, &SerialWorker::commandFinished, this, [this](const QString &command, bool accepted) {
        if (const auto request = Protocol::parseCommand(command))
            emit commandFinished(request->command, accepted);
    });
    connect(worker, &SerialWorker::startupFailed, this, [this](const QString &text) {
        connectionWanted_ = false;
        startupError_ = text;
    });
    connect(worker, &SerialWorker::commandsReady, this, [this] {
        auto state = state_; state.initialized = state.connected; setSystemState(state);
    });
    connect(worker, &SerialWorker::systemStatusReceived, this, &SerialController::updateDeviceStatus);
    connect(worker, &SerialWorker::streamObserved, this, [this](bool accelerometer) {
        if (!state_.canSendCommands()) return;
        auto state = state_;
        if (accelerometer) state.accelerometer = true;
        else state.measuring = true;
        setSystemState(state);
    });
    connect(worker, &SerialWorker::deviceMessage, this, [this](char type, quint8 code, qint32 value) {
        MessageEvent message; message.type = type; message.code = code; message.value = value;
        if (type == 'E') emit deviceError(message.content());
        else if (type == 'W') emit deviceWarning(message.content());
    });
    connect(worker, &SerialWorker::commandSent, this, &SerialController::commandSent);
    connect(worker, &SerialWorker::commandReply, this,
            [this](const QString &command, char type, quint8 code, qint32 value) {
        MessageEvent message; message.type = type; message.code = code; message.value = value;
        emit commandResult(command, message.content());
    });
    connect(worker, &SerialWorker::deviceWarning, this, &SerialController::deviceWarning);
    connect(worker, &SerialWorker::stateReceived, this, &SerialController::stateReceived);
    connect(worker, &SerialWorker::commandResult, this, &SerialController::commandResult);
    // quit est thread-safe : le GUI peut attendre le thread lors de sa fermeture.
    connect(worker, &SerialWorker::finished, thread_, &QThread::quit, Qt::DirectConnection);
    connect(thread_, &QThread::finished, worker, &QObject::deleteLater);
    auto current = thread_;
    connect(current, &QThread::finished, this, [this, current] {
        if (thread_ != current) return;
        const bool unexpected = state_.connected && connectionWanted_ && startupError_.isEmpty();
        const QString disconnectedPort = activePort_;
        thread_ = nullptr; current->deleteLater();
        auto state = state_;
        state.connected = state.initialized = state.measuring = state.accelerometer = false;
        state.port = selectedPort(); state.baud = baud;
        setSystemState(state);
        if (!startupError_.isEmpty()) {
            const auto error = startupError_; startupError_.clear();
            emit fault(error);
        }
        if (unexpected) {
            QString message = tr("La connexion au port série %1 a été interrompue de manière inattendue. Vérifiez le branchement de la carte.").arg(disconnectedPort);
            emit fault(message);
            emit connectionLost(message);
        }
        // Le timer impose un délai avant une éventuelle nouvelle tentative.
        emit portsChanged();
    });
    thread_->start(); emit portsChanged();
}
void SerialController::disconnectPort() {
    connectionWanted_ = false;
    if (thread_) emit stopRequested();
}
void SerialController::shutdown() {
    connectionWanted_ = false;
    if (!thread_) return;
    emit stopRequested();
    thread_->wait(); // arrêt borné des E/S dans stop(), pas de terminate().
    delete thread_; thread_ = nullptr;
}
void SerialController::sendCommand(Protocol::CMD command, const QStringList &values, bool awaitReply) {
    for (const auto &value : values) {
        if (value.isEmpty() || std::any_of(value.cbegin(), value.cend(), [](QChar ch) { return ch.isSpace(); })) {
            emit fault(tr("Valeur de commande invalide."));
            return;
        }
    }
    const Protocol::CommandRequest request{command, values};
    if (settings_.command(command).isEmpty()) { emit fault(tr("Commande inconnue.")); return; }
    sendRequest(request.text(), awaitReply);
    if (!canSendCommands()) emit commandFinished(command, false);
}

void SerialController::sendRawCommand(const QString &command) {
    sendRequest(command, false);
}

void SerialController::sendRequest(const QString &command, bool awaitReply) {
    if (canSendCommands()) emit sendRequested(command, awaitReply);
    else if (state_.connected) emit commandResult(command, "Envoi refusé : attente de la fin d'initialisation (code 71)");
    else emit fault("Commande non envoyée : carte déconnectée");
}

void SerialController::setAcquisition(bool on) {
    if (!canSendCommands()) return;
    sendCommand(on ? Protocol::CMD::MesuresOn : Protocol::CMD::MesuresOff);
}

void SerialController::setAccelerometer(bool on) {
    if (!canSendCommands()) {
        emit commandFinished(on ? Protocol::CMD::AccelerometreOn : Protocol::CMD::AccelerometreOff, false);
        return;
    }
    sendCommand(on ? Protocol::CMD::AccelerometreOn : Protocol::CMD::AccelerometreOff, {}, true);
}
