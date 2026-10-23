#include "serial/SerialWorker.h"
#include "serial/Protocol.h"
#include "core/AppSettings.h"
#include <QElapsedTimer>

SerialWorker::SerialWorker(std::shared_ptr<SharedQueues> queues)
    : queues_(std::move(queues)) {}

void SerialWorker::open(const QString &name, qint32 baud) {
    // Ces objets sont créés après moveToThread, dans le thread de travail.
    port_ = new QSerialPort(this);
    resetTimer_ = new QTimer(this); resetTimer_->setSingleShot(true);
    connect(resetTimer_, &QTimer::timeout, this, [this] {
        emit startupFailed("Erreur : vérifiez le port série.");
        stop();
    });
    startupTimer_ = new QTimer(this); startupTimer_->setSingleShot(true);
    connect(startupTimer_, &QTimer::timeout, this, [this] {
        // Seule exception à l'attente du code 71 : relancer une carte silencieuse.
        sendCommand(AppSettings::instance().command(Protocol::CMD::Reset), false);
        if (!stopping_ && !commandsReady_) resetTimer_->start(5000);
    });
    replyTimer_ = new QTimer(this); replyTimer_->setSingleShot(true);
    connect(replyTimer_, &QTimer::timeout, this, [this] {
        const auto command = pending_; pending_.clear();
        emit commandResult(command, "Délai de réponse dépassé");
        emit commandFinished(command, false);
        flushDeferredStops();
    });
    connect(port_, &QSerialPort::readyRead, this, &SerialWorker::receive);
    connect(port_, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (stopping_ || error == QSerialPort::NoError) return;
        if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError ||
            error == QSerialPort::ReadError || error == QSerialPort::WriteError) {
            emit fault(port_->errorString()); stop();
        }
    });
    port_->setPortName(name);
    if (!port_->setBaudRate(baud) || !port_->setDataBits(QSerialPort::Data8) ||
        !port_->setParity(QSerialPort::NoParity) || !port_->setStopBits(QSerialPort::OneStop) ||
        !port_->setFlowControl(QSerialPort::NoFlowControl) || !port_->open(QIODevice::ReadWrite)) {
        emit fault(port_->errorString()); stop(); return;
    }
    if (!stopping_) {
        // Mega's USB bridge couples DTR to RESET. Deassert, then assert it
        // to produce a falling electrical edge even when the port was already open.
        resetInProgress_ = true;
        emit resetDetected();
        port_->clear(QSerialPort::Input);
        const bool dtrReleased = port_->setDataTerminalReady(false);
        port_->setRequestToSend(false);
        QTimer::singleShot(100, this, [this, dtrReleased] {
            if (stopping_ || !port_ || !port_->isOpen()) return;
            const bool dtrAsserted = port_->setDataTerminalReady(true);
            port_->setRequestToSend(true);
            if (stopping_) return;
            decoder_.reset();
            resetInProgress_ = false;
            // Keep the existing software reset fallback for silent boards.
            startupTimer_->start(2000);
            resetTimer_->start(7000);
            emit opened();
            if (!dtrReleased || !dtrAsserted)
                emit deviceWarning(tr("Reset DTR indisponible ; attente du démarrage puis secours par DO RESET."));
        });
    }
}

void SerialWorker::send(const QString &command, bool awaitReply) {
    if (!commandsReady_) return;
    sendCommand(command, awaitReply);
}

void SerialWorker::sendCommand(const QString &command, bool awaitReply) {
    if (stopping_ || !port_ || !port_->isOpen()) return;
    const QString clean = command.trimmed();
    if (clean.isEmpty() || clean.contains('\n') || clean.contains('\r')) {
        return;
    }
    // Le protocole n'a pas d'identifiant de corrélation : une seule commande
    // suivie à la fois, aucun autre envoi tant qu'elle attend sa réponse.
    if (!pending_.isEmpty()) {
        // Tab changes must still stop the device after the current reply (or its timeout).
        if (!awaitReply && clean == AppSettings::instance().command(Protocol::CMD::Stop)) { deferredStop_ = true; return; }
        if (clean == AppSettings::instance().command(Protocol::CMD::MesuresOff)) {
            deferredMeasuresOff_ = true;
            deferredMeasuresOffAwaitReply_ = deferredMeasuresOffAwaitReply_ || awaitReply;
            return;
        }
        emit commandFinished(clean, false);
        emit commandResult(clean, "Envoi refusé : une commande attend sa réponse"); return;
    }
    const QByteArray bytes = clean.toUtf8() + '\n';
    if (awaitReply) { pending_ = clean; replyTimer_->start(2000); }
    const auto count = port_->write(bytes);
    if (stopping_) return;
    if (count != bytes.size()) {
        if (awaitReply) { pending_.clear(); replyTimer_->stop(); }
        emit fault("Écriture série incomplète"); stop(); return;
    }
    auto e = std::make_shared<RequestEvent>(); e->command = clean; e->raw = bytes;
    queues_->events.push(e); // signifie mis en tampon, pas acquitté par la carte.
    emit commandSent(clean);
    if (clean == AppSettings::instance().command(Protocol::CMD::Reset)) {
        deferredStop_ = deferredMeasuresOff_ = false;
        deferredMeasuresOffAwaitReply_ = false;
        commandsReady_ = false;
        measuresObserved_ = accelerometerObserved_ = false;
        emit resetDetected();
        resetTimer_->start(7000);
    }
}
void SerialWorker::flushDeferredStops() {
    const bool stopPilotage = deferredStop_, stopMeasures = deferredMeasuresOff_;
    const bool awaitMeasuresOff = deferredMeasuresOffAwaitReply_;
    deferredStop_ = deferredMeasuresOff_ = false;
    deferredMeasuresOffAwaitReply_ = false;
    if (!commandsReady_ || stopping_) return;
    if (stopPilotage) sendCommand(AppSettings::instance().command(Protocol::CMD::Stop), false);
    if (stopMeasures) sendCommand(AppSettings::instance().command(Protocol::CMD::MesuresOff), awaitMeasuresOff);
}

void SerialWorker::receive() {
    const QString settingsPath;
    const auto &settings = AppSettings::instance(settingsPath);
    if (resetInProgress_) { port_->readAll(); return; }
    for (const auto &e : decoder_.feed(port_->readAll())) {
        if (commandsReady_ && e->kind() == Event::Kind::Measure && !measuresObserved_) {
            measuresObserved_ = true;
            emit streamObserved(false);
        }
        if (commandsReady_ && e->kind() == Event::Kind::Accelerometer && !accelerometerObserved_) {
            accelerometerObserved_ = true;
            emit streamObserved(true);
        }
        if (e->kind() == Event::Kind::Measure || e->kind() == Event::Kind::Accelerometer)
            queues_->measures.push(e);
        else queues_->events.push(e);
        const auto m = std::dynamic_pointer_cast<const MessageEvent>(e);
        if (!m) continue;
        if (!commandsReady_ && m->type == 'I'
            && m->code == settings.codeId("INFO_VERSION_SOFT") && m->value < 2) {
            emit startupFailed(tr("Connexion refusée : version du logiciel de la carte incompatible (%1). Version minimale requise : 1.")
                                   .arg(m->value));
            stop();
            return;
        }
        if (m->type == 'O' || m->type == 'I' || m->type == 'R') {
            if (m->code == settings.codeId("INFO_MESURES_DESACTIVE")
                || (m->code == settings.codeId("DATA_FREQ_MESURES") && m->value <= 0)) measuresObserved_ = false;
            if ((m->code == settings.codeId("INFO_ACCEL_SET") || m->code == settings.codeId("INFO_ACCEL_GET"))
                && m->value == 0) accelerometerObserved_ = false;
        }
        if (m->code == settings.codeId("INFO_ACCEL_SET")
            || m->code == settings.codeId("INFO_ACCEL_GET")
            || m->code == settings.codeId("ERR_ACCEL_ABSENT")
            || m->code == settings.codeId("INFO_MESURES_DESACTIVE")
            || m->code == settings.codeId("DATA_FREQ_MESURES")) {
            emit systemStatusReceived(m->type, m->code, m->value);
        }
        if (m->type == 'I' && (m->code == settings.codeId("INFO_DO_RESET") || m->code == settings.codeId("INFO_INIT_DEBUT"))) {
            deferredStop_ = deferredMeasuresOff_ = false;
            deferredMeasuresOffAwaitReply_ = false;
            startupTimer_->stop();
            commandsReady_ = false;
            measuresObserved_ = accelerometerObserved_ = false;
            if (!pending_.isEmpty()) {
                const auto command = pending_; pending_.clear(); replyTimer_->stop();
                emit commandResult(command, tr("Commande interrompue par un reset"));
                emit commandFinished(command, false);
            }
            emit resetDetected();
            resetTimer_->start(7000);
        }
        if (m->type == 'I' && m->code == settings.codeId("INFO_INIT_FIN") && !commandsReady_) {
            startupTimer_->stop();
            resetTimer_->stop();
            commandsReady_ = true;
            emit commandsReady();
        }
        if (m->type == 'E' || m->type == 'W') emit deviceMessage(m->type, m->code, m->value);
        if (m->type == 'S') emit stateReceived(m->value);
        if ((m->type == 'O' || m->type == 'N') && !pending_.isEmpty()) {
            replyTimer_->stop(); const auto command = pending_; pending_.clear();
            emit commandReply(command, m->type, m->code, m->value);
            emit commandFinished(command, m->type == 'O');
            flushDeferredStops();
        }
    }
}
void SerialWorker::stop() {
    if (stopping_) return;
    stopping_ = true;
    if (replyTimer_) replyTimer_->stop();
    if (startupTimer_) startupTimer_->stop();
    if (resetTimer_) resetTimer_->stop();
    if (!pending_.isEmpty()) { emit commandResult(pending_, "Connexion fermée"); pending_.clear(); }
    if (port_ && port_->isOpen()) {
        // Tentative bornée dans le worker ; aucune garantie si la carte est retirée.
        const auto bytes = AppSettings::instance().command(Protocol::CMD::MesuresOff).toUtf8() + '\n';
        if (commandsReady_ && port_->write(bytes) == bytes.size()) {
            auto e = std::make_shared<RequestEvent>(); e->command = AppSettings::instance().command(Protocol::CMD::MesuresOff); e->raw = bytes;
            queues_->events.push(e);
        }
        // waitForBytesWritten peut ne vider qu'une partie des données.
        QElapsedTimer deadline; deadline.start();
        while (port_->bytesToWrite() > 0 && deadline.elapsed() < 300)
            if (!port_->waitForBytesWritten(300 - int(deadline.elapsed()))) break;
        port_->close();
    }
    decoder_.reset(); emit finished();
}
