#pragma once
#include "serial/SerialDecoder.h"
#include "core/EventQueue.h"
#include <QObject>
#include <QSerialPort>
#include <QTimer>
class SerialWorker final : public QObject {
    Q_OBJECT
public:
    explicit SerialWorker(std::shared_ptr<SharedQueues> queues);
public slots:
    void open(const QString &name, qint32 baud);
    void send(const QString &command, bool awaitReply = false);
    void stop();
signals:
    void opened();
    void commandsReady();
    void resetDetected();
    void commandFinished(const QString &command, bool accepted);
    void commandSent(const QString &command);
    void finished();
    void fault(const QString &text);
    void startupFailed(const QString &text);
    void stateReceived(int state);
    void systemStatusReceived(char type, quint8 code, qint32 value);
    void streamObserved(bool accelerometer);
    void deviceWarning(const QString &text);
    void deviceMessage(char type, quint8 code, qint32 value);
    void commandReply(const QString &command, char type, quint8 code, qint32 value);
    void commandResult(const QString &command, const QString &result);
private:
    void receive();
    void sendCommand(const QString &command, bool awaitReply);
    void flushDeferredStops();
    std::shared_ptr<SharedQueues> queues_;
    SerialDecoder decoder_;
    QSerialPort *port_ = nullptr;
    QTimer *replyTimer_ = nullptr;
    QTimer *startupTimer_ = nullptr;
    QTimer *resetTimer_ = nullptr;
    QString pending_;
    bool deferredStop_ = false, deferredMeasuresOff_ = false;
    bool deferredMeasuresOffAwaitReply_ = false;
    bool stopping_ = false;
    bool commandsReady_ = false;
    bool resetInProgress_ = false;
    bool measuresObserved_ = false, accelerometerObserved_ = false;
};
