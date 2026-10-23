#pragma once
#include "serial/SerialWorker.h"
#include "core/SystemState.h"
#include "core/AppSettings.h"
#include "serial/Protocol.h"
#include <QThread>
#include <QSerialPortInfo>
#include <QJsonObject>
class SerialController final : public QObject {
    Q_OBJECT
public:
    explicit SerialController(QObject *parent = nullptr, const QString &settingsPath = QString());
    ~SerialController() override;
    std::shared_ptr<SharedQueues> queues() const { return queues_; }
    QList<QSerialPortInfo> ports() const { return ports_; }
    const SystemState &systemState() const { return state_; }
    bool connected() const { return state_.connected; }
    bool canSendCommands() const { return state_.canSendCommands(); }
    bool busy() const { return thread_ != nullptr; }
    QString selectedPort() const;
    bool automatic = true;
    QString manualPort;
    qint32 baud = 115200;
    
    bool saveSettings(QString *error = nullptr);
    // Applique uniquement les paramètres série demandés ; l'état matériel reste contrôlé ici.
    bool applySerialSettings(bool automaticMode, const QString &port, qint32 baudRate, QString *error = nullptr);

public slots:
    void scan();
    void connectPort();
    void disconnectPort();
    void sendRawCommand(const QString &command);
    // Assemble la commande du JSON et ses arguments, puis utilise le suivi O/N existant.
    void sendCommand(Protocol::CMD command, const QStringList &values = {}, bool awaitReply = false);
    void setAcquisition(bool on);
    void setAccelerometer(bool on);
signals:
    void systemStateChanged(const SystemState &state);
    void portsChanged();
    void connectionChanged(bool connected, const QString &port);
    void connectionLost(const QString &text);
    void commandsReady();
    void resetDetected();
    void accelerometerChanged(bool on);
    void commandFinished(Protocol::CMD command, bool accepted);
    void commandSent(const QString &command);
    void fault(const QString &text);
    void stateReceived(int state);
    void deviceWarning(const QString &text);
    void deviceError(const QString &text);
    void resetErrors();
    void commandResult(const QString &command, const QString &result);
    void acquisitionRequested(bool on);
    void sendRequested(const QString &command, bool awaitReply);
    void stopRequested();
private:
    const AppSettings &settings_;
    void setSystemState(SystemState state);
    void updateDeviceStatus(char type, quint8 code, qint32 value);
    void shutdown();
    void sendRequest(const QString &command, bool awaitReply);
    std::shared_ptr<SharedQueues> queues_ = std::make_shared<SharedQueues>();
    QList<QSerialPortInfo> ports_;
    QTimer scanTimer_;
    QThread *thread_ = nullptr;
    SystemState state_;
    bool connectionWanted_ = false;
    QString activePort_;
    QString startupError_;
    QString lastSerialError_;
};
