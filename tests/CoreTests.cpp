#include "core/DebugStyles.h"
#include <QtTest>
#include "serial/SerialDecoder.h"
#include "core/EventQueue.h"
#include "core/MeasurementStore.h"
#include "core/AppSettings.h"
#include "serial/Protocol.h"
#include <QTemporaryDir>
#include <QtEndian>
#include <thread>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonArray>
namespace {
quint8 frameCrc(const QByteArray &bytes) {
    quint8 crc = 0;
    for (char byte : bytes) {
        crc ^= quint8(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80) ? quint8((crc << 1) ^ 0x07) : quint8(crc << 1);
    }
    return crc;
}
void finishFrame(QByteArray &bytes) {
    bytes.append(char(frameCrc(bytes)));
    bytes.append('\n');
}
template<class T> void add(QByteArray &b, T value) {
    const auto start = b.size(); b.resize(start + sizeof(T)); qToLittleEndian<T>(value, b.data() + start);
}
QByteArray message(char type = 'S', quint8 code = 251, qint32 value = -123) {
    QByteArray b(1, '\1'); b.append(type); add(b, code); add(b, value); finishFrame(b); return b;
}
QByteArray measure() {
    QByteArray b(1, '\1'); b.append('M'); add<quint32>(b, 0xfffffffeu); add<qint16>(b, -2400);
    add<qint16>(b, -123); add<qint32>(b, -123456); add<qint32>(b, 567890);
    add<qint32>(b, 35999); add<qint32>(b, -987654); finishFrame(b); return b;
}
QByteArray accelerometer() {
    QByteArray b(1, '\1'); b.append('A'); add<quint32>(b, 25);
    for (qint16 v : {-32768, 0, 32767}) add(b, v);
    for (qint16 v : {-10000, 0, 10000, -20000, 0, 20000}) add(b, v);
    finishFrame(b);
    return b;
}
}
class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void protocolJson() {
        QFile file(":/protocol.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto json = QJsonDocument::fromJson(file.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(json.isArray());
        const auto commands = json.array();
        QVERIFY(!commands.isEmpty());
        QCOMPARE(AppSettings::instance().command(Protocol::CMD::Reset), QString("DO RESET"));
        QCOMPARE(Protocol::commandName(Protocol::CMD::Reset), QString("Reset"));
        const auto reset = Protocol::commandId("Reset");
        QVERIFY(reset.has_value());
        QCOMPARE(*reset, Protocol::CMD::Reset);
        QVERIFY(!Protocol::commandId("Inconnue"));
        QVERIFY(AppSettings::instance().command("Inconnue").isEmpty());
        const Protocol::CommandRequest unknown{static_cast<Protocol::CMD>(-1), {"1"}};
        QVERIFY(unknown.text().isEmpty());
        for (const auto &entry : commands) {
            const auto object = entry.toObject();
            QCOMPARE(AppSettings::instance().command(object.value("key").toString()), object.value("cmd").toString());
            const auto parsedCommand = Protocol::parseCommand(object.value("cmd").toString());
            QVERIFY(parsedCommand.has_value());
            QCOMPARE(Protocol::commandName(parsedCommand->command), object.value("key").toString());
        }
        QCOMPARE(AppSettings::instance().command(Protocol::CMD::ConsigneEchelon), QString("SET CONSIGNE ECHELON"));
        const Protocol::CommandRequest request{Protocol::CMD::SetPidVitesse, {"1", "0.5", "-2"}};
        QCOMPARE(request.text(), QString("SET PID VITESSE 1 0.5 -2"));
        const auto parsed = Protocol::parseCommand(request.text());
        QVERIFY(parsed.has_value()); QCOMPARE(parsed->command, request.command); QCOMPARE(parsed->values, request.values);
        QVERIFY(!Protocol::parseCommand("DO UNKNOWN"));
        QCOMPARE(AppSettings::instance().codeId("INFO_INIT_FIN"), 71);
        QCOMPARE(Protocol::Etat::PROD_PILOTAGE, 5);
    }
    void csvStateReferences() {
        QString error;
        QVERIFY2(AppSettings::initialize(&error), qPrintable(error));
        const auto &settings = AppSettings::instance();
        QCOMPARE(settings.stateId("PROD_INIT"), 0);
        QCOMPARE(settings.stateId("PROD_OUVERTURE"), Protocol::Etat::PROD_OUVERTURE);
        QCOMPARE(settings.stateId("CALI_OUVERTURE"), Protocol::Etat::CALI_OUVERTURE);
        QCOMPARE(settings.stateId("PROD_CALIBRATION"), Protocol::Etat::PROD_CALIBRATION);
        QCOMPARE(settings.stateId("INCONNU"), -1);
        QCOMPARE(settings.stateName(Protocol::Etat::PROD_FONCTIONNEMENT), QString("FONCTIONNEMENT"));
        QCOMPARE(settings.stateName(Protocol::Etat::CALI_FIN), QString("FIN"));
        const auto machines = settings.states().value("machines").toArray();
        const auto calibration = machines[1].toObject();
        QCOMPARE(calibration.value("default_state").toInt(), Protocol::Etat::CALI_FIN);
        QCOMPARE(calibration.value("display_when").toObject().value("state").toInt(), Protocol::Etat::PROD_CALIBRATION);
        const auto first = calibration.value("etats").toArray().first().toObject();
        QCOMPARE(first.value("id").toInt(), 0);
        QCOMPARE(first.value("etat").toString(), QString("INIT"));
        QCOMPARE(first.value("code").toInt(), Protocol::Etat::CALI_INIT);
        const auto transition = calibration.value("transitions").toArray().first().toObject();
        QCOMPARE(transition.value("start").toInt(), Protocol::Etat::CALI_INIT);
        QCOMPARE(transition.value("end").toInt(), Protocol::Etat::CALI_OUVERTURE);
    }
    void applicationSettings() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("config.ini");
        const auto &settings = AppSettings::instance(path);
        QVERIFY(!QFile::exists(path));
        QCOMPARE(settings.value("serial/automatic").toBool(), true);
        QCOMPARE(settings.value("serial/baud").toInt(), 115200);
        QCOMPARE(settings.value("serial/port").toString(), QString("COM1"));
        QVERIFY(!settings.openFolder().isEmpty());
        QString error;
        QVERIFY2(settings.save({{"serial/automatic", false}, {"serial/port", "COM42"},
                                {"custom/keep", "preserved"}}, &error), qPrintable(error));
        QVERIFY(QFile::exists(path));
        const auto &reloaded = AppSettings::instance(path);
        QCOMPARE(&reloaded, &settings);
        QCOMPARE(&reloaded.codes(), &AppSettings::instance().codes());
        QCOMPARE(&reloaded.debugStyles(), &AppSettings::instance().debugStyles());
        QVERIFY(!reloaded.value("serial/automatic").toBool());
        QCOMPARE(reloaded.value("serial/port").toString(), QString("COM42"));
        QCOMPARE(reloaded.value("serial/baud").toInt(), 115200);
        QVERIFY(settings.save({{"serial/baud", 57600}, {"files/openFolder", "data"}}));
        QCOMPARE(reloaded.value("serial/baud").toInt(), 57600);
        QCOMPARE(reloaded.openFolder(), dir.filePath("data"));
        QCOMPARE(reloaded.value("custom/keep").toString(), QString("preserved"));
        QVERIFY(settings.save({{"serial/baud", "invalid"}, {"serial/automatic", "invalid"}}));
        QCOMPARE(reloaded.value("serial/baud").toInt(), 115200);
        QVERIFY(reloaded.value("serial/automatic").toBool());
        const auto &unwritable = AppSettings::instance(dir.path()); // A directory cannot be replaced by an INI file.
        QVERIFY(!unwritable.save({{"serial/baud", 9600}}, &error));
        QVERIFY(!error.isEmpty());
    }
    void initTestCase() {
        QString error;
        QVERIFY2(AppSettings::initialize(&error), qPrintable(error));
    }
    void debugStylesConfiguration() {
        DebugStyles styles;
        QString error;
        const QByteArray json = "{\"types\":[{\"type\":\"E\",\"foreground\":\"#ff0000de\","
            "\"background\":\"#112233\",\"justify\":\"right\",\"underline\":false,"
            "\"overstrike\":true,\"prefix\":null,\"message\":\"Erreur : $\"}]}";
        const auto root = QJsonDocument::fromJson(json).object();
        QVERIFY2(styles.load(root, &error), qPrintable(error));
        QVERIFY(!styles.hasPrefixes());
        const auto &style = styles.style('E');
        QCOMPARE(style.characters.foreground().color(), QColor(255, 0, 0, 222));
        QCOMPARE(style.characters.background().color(), QColor("#112233"));
        QCOMPARE(style.alignment, Qt::Alignment(Qt::AlignRight));
        QVERIFY(style.characters.fontStrikeOut());
        QVERIFY(!style.characters.fontUnderline());
        QCOMPARE(style.message, QString("Erreur : $"));
        QVERIFY(styles.style('?').characters.isEmpty());
        QVERIFY(styles.load(QJsonDocument::fromJson(R"({"types":[{"type":"Q","prefix":"\ud83d\ude00"}]})").object(), &error));
        QVERIFY(styles.hasPrefixes()); // One Unicode character may occupy two UTF-16 code units.
    }
    void invalidDebugStyles_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("invalid message") << QByteArray(R"({"types":[{"type":"E","message":12}]})");
        QTest::newRow("long prefix") << QByteArray(R"({"types":[{"type":"E","prefix":"!!"}]})");
        QTest::newRow("newline prefix") << QByteArray(R"({"types":[{"type":"E","prefix":"\n"}]})");
        QTest::newRow("invalid color") << QByteArray(R"({"types":[{"type":"E","foreground":"invalid"}]})");
        QTest::newRow("invalid alignment") << QByteArray(R"({"types":[{"type":"E","justify":"middle"}]})");
        QTest::newRow("invalid boolean") << QByteArray(R"({"types":[{"type":"E","underline":"yes"}]})");
        QTest::newRow("duplicate type") << QByteArray(R"({"types":[{"type":"E"},{"type":"E"}]})");
        QTest::newRow("missing types") << QByteArray("{}");
    }
    void invalidDebugStyles() {
        QFETCH(QByteArray, json);
        DebugStyles styles;
        QString error;
        QVERIFY(!styles.load(QJsonDocument::fromJson(json).object(), &error));
        QVERIFY(error.startsWith("debug.json : "));
    }
    void sharedConfigurationAndNumericMessages() {
        const auto *configuration = &AppSettings::instance();
        QVERIFY(AppSettings::initialize());
        QCOMPARE(&AppSettings::instance(), configuration);
        QVERIFY(!configuration->states().isEmpty());
        QVERIFY(!configuration->curves().isEmpty());
        QVERIFY(!configuration->curves().contains("messages"));
        QVERIFY(configuration->debugStyles().hasPrefixes());
        QCOMPARE(configuration->debugStyles().style('E').message, QString("Erreur : $"));
        MeasureEvent measured;
        measured.time = 1234;
        QCOMPARE(measured.rawValues()[0], qint64(1234));
        QCOMPARE(measured.content(), QString("1234 0 0 0 0 0 0"));
        QCOMPARE(measured.rawValues(), (std::array<qint64, 7>{1234, 0, 0, 0, 0, 0, 0}));
        AccelerometerEvent acceleration;
        acceleration.time = 42; acceleration.velocity = {-123, 456, -789};
        QCOMPARE(acceleration.rawValues(), (std::array<qint64, 10>{42, 0, 0, 0, -123, 456, -789, 0, 0, 0}));
        SerialDecoder decoder;
        const auto events = decoder.feed(message('E', 1, 42));
        QCOMPARE(events.size(), 1);
        const auto event = std::dynamic_pointer_cast<const MessageEvent>(events.first());
        QVERIFY(event);
        QCOMPARE(event->code, quint8(1));
        QCOMPARE(event->value, 42);
        QCOMPARE(event->content(), configuration->codes().format(1, 42));
        QVERIFY(event->content().contains(configuration->codes().format(1, 42)));
    }
    void namedStateEvents() {
        const auto &configuration = AppSettings::instance();
        QCOMPARE(configuration.stateName(1), QString("REPOS"));
        QCOMPARE(configuration.stateName(28), QString("FIN"));
        StateEvent unknown;
        for (qint32 number : {-1, 19, 999}) {
            unknown.value = number;
            QCOMPARE(unknown.content(), QString("État inconnu"));
            QCOMPARE(configuration.stateName(number), unknown.content());
        }
        StateEvent mismatched;
        mismatched.code = 250; mismatched.value = 28;
        QCOMPARE(mismatched.content(), QString("FIN"));
        SerialDecoder decoder;
        const auto events = decoder.feed(message('S', 250, 1) + message('S', 251, 28)
            + message('S', 250, 6) + message('S', 250, 999) + message('S', 254, 999)
            + message('I', 250, 1));
        QCOMPARE(events.size(), 6);
        auto state = std::dynamic_pointer_cast<const StateEvent>(events[0]); QVERIFY(state);
        QCOMPARE(state->code, quint8(250)); QCOMPARE(state->value, 1);
        QCOMPARE(state->raw, message('S', 250, 1).chopped(1));
        QCOMPARE(state->content(), QString("REPOS"));
        QCOMPARE(events[1]->content(), QString("FIN"));
        QCOMPARE(events[2]->content(), QString("CALIBRATION"));
        QCOMPARE(events[3]->content(), QString("État inconnu"));
        QCOMPARE(events[4]->content(), QString("État inconnu"));
        QVERIFY(!std::dynamic_pointer_cast<const StateEvent>(events[5]));
    }
    void allSplitPoints() {
        const QByteArray input = message() + measure() + accelerometer() + "Arduino pret\r\n";
        for (qsizetype i = 0; i <= input.size(); ++i) {
            SerialDecoder decoder; auto out = decoder.feed(input.first(i)); out += decoder.feed(input.mid(i));
            QCOMPARE(out.size(), 4);
            auto m = std::dynamic_pointer_cast<const MessageEvent>(out[0]); QVERIFY(m);
            QCOMPARE(m->value, -123); QCOMPARE(m->code, quint8(251));
            auto v = std::dynamic_pointer_cast<const MeasureEvent>(out[1]); QVERIFY(v);
            QCOMPARE(v->time, quint32(0xfffffffeu)); QCOMPARE(v->voltage, qint16(-2400));
            QCOMPARE(v->current, qint16(-123)); QCOMPARE(v->motorAngle, -123456);
            QCOMPARE(v->motorSpeed, 567890); QCOMPARE(v->doorAngle, quint16(35999));
            QCOMPARE(v->target, -987654);
            auto a = std::dynamic_pointer_cast<const AccelerometerEvent>(out[2]); QVERIFY(a);
            QCOMPARE(a->acceleration[0], qint16(-32768)); QCOMPARE(a->angles[2], qint16(20000));
            QCOMPARE(out[3]->content(), QString("Arduino pret"));
        }
    }
    void byteByByte() {
        SerialDecoder decoder; QVector<EventPtr> out;
        const auto bytes = message('I', 11, 11) + measure() + accelerometer();
        for (char byte : bytes) out += decoder.feed(QByteArray(1, byte));
        QCOMPARE(out.size(), 3); QCOMPARE(out[0]->raw.size(), 8);
        QCOMPARE(out[1]->raw.size(), 27); QCOMPARE(out[2]->raw.size(), 25);
    }
    void signedMotorVoltage() {
        for (qint16 voltage : {qint16(-2500), qint16(0), qint16(2500)}) {
            auto bytes = measure();
            QCOMPARE(bytes.size(), 28);
            qToLittleEndian<qint16>(voltage, bytes.data() + 6); bytes[bytes.size() - 2] = char(frameCrc(bytes.first(bytes.size() - 2)));
            SerialDecoder decoder;
            const auto events = decoder.feed(bytes + message());
            QCOMPARE(events.size(), 2);
            const auto decoded = std::dynamic_pointer_cast<const MeasureEvent>(events.first());
            QVERIFY(decoded);
            QCOMPARE(decoded->voltage, voltage);
            QCOMPARE(decoded->target, -987654);
            QCOMPARE(decoded->rawValues()[6], qint64(-987654));
            QVERIFY(!decoded->content().contains("PWM="));
        }
    }
    void partialResetAndLongAscii() {
        SerialDecoder decoder; QVERIFY(decoder.feed(measure().first(10)).isEmpty());
        decoder.reset(); auto result = decoder.feed("hello\n"); QCOMPARE(result.size(), 1);
        QVERIFY(decoder.feed(QByteArray(4097, 'x')).isEmpty());
        result = decoder.feed("\n"); QCOMPARE(result.size(), 1);
        QCOMPARE(result[0]->raw.size(), 4098);
    }
    void textContainingBinaryMarker() {
        const auto bytes = QByteArray("sans retour ligne") + message('T', 106, 1)
            + "suite\r\n" + measure() + accelerometer();
        for (qsizetype i = 0; i <= bytes.size(); ++i) {
            SerialDecoder decoder;
            auto out = decoder.feed(bytes.first(i)); out += decoder.feed(bytes.mid(i));
            QCOMPARE(out.size(), 4);
            QVERIFY(std::dynamic_pointer_cast<const TextEvent>(out[0]));
            QCOMPARE(out[0]->raw, QByteArray("sans retour ligne") + message('T', 106, 1));
            QCOMPARE(out[1]->content(), QString("suite"));
            QCOMPARE(out[2]->raw, measure().chopped(1)); QCOMPARE(out[3]->raw, accelerometer().chopped(1));
        }
    }
    void binaryMarkersInPayload() {
        SerialDecoder decoder;
        for (char type : {'E', 'W', 'I', 'S', 'R', 'O', 'N', 'T'}) {
            const auto bytes = message(type, 10, 0x010a010a);
            const auto out = decoder.feed(bytes);
            QCOMPARE(out.size(), 1); QCOMPARE(out[0]->raw, bytes.chopped(1));
            auto event = std::dynamic_pointer_cast<const MessageEvent>(out[0]); QVERIFY(event);
            QCOMPARE(event->type, type); QCOMPARE(event->value, 0x010a010a);
        }
    }
    void binaryLengthsAndSuffixes() {
        auto motor = measure().chopped(1);
        qToLittleEndian<qint32>(0x010a010a, motor.data() + 18); motor.back() = char(frameCrc(motor.chopped(1)));
        auto inertial = accelerometer().chopped(1);
        qToLittleEndian<qint16>(10, inertial.data() + 6); inertial.back() = char(frameCrc(inertial.chopped(1)));
        const auto msg = message().chopped(1);
        const auto bytes = motor + inertial + "suffixe\1ignore\r\ntexte\n" + msg;
        for (qsizetype i = 0; i <= bytes.size(); ++i) {
            SerialDecoder decoder;
            auto out = decoder.feed(bytes.first(i)); out += decoder.feed(bytes.mid(i));
            QCOMPARE(out.size(), 5);
            QCOMPARE(out[0]->raw, motor); QCOMPARE(out[1]->raw, inertial);
            QCOMPARE(out[2]->raw, QByteArray("suffixe\1ignore\r\n")); QCOMPARE(out[3]->content(), QString("texte")); QCOMPARE(out[4]->raw, msg);
            const auto decoded = std::dynamic_pointer_cast<const MeasureEvent>(out[0]);
            QVERIFY(decoded); QCOMPARE(decoded->doorAngle, 0x010a010a);
            decoder.reset();
            const auto text = decoder.feed("apres reset\n");
            QCOMPARE(text.size(), 1); QCOMPARE(text[0]->content(), QString("apres reset"));
        }
    }
    void invalidCrcAndFollowingText() {
        QCOMPARE(frameCrc("123456789"), quint8(0xf4));
        for (auto frame : {message().chopped(1), measure().chopped(1), accelerometer().chopped(1)}) {
            frame.back() = char(quint8(frame.back()) ^ 0x80);
            const auto bytes = frame + message().chopped(1) + "texte apres CRC\n";
            for (qsizetype split = 0; split <= bytes.size(); ++split) {
                SerialDecoder decoder;
                auto out = decoder.feed(bytes.first(split));
                out += decoder.feed(bytes.mid(split));
                QCOMPARE(out.size(), 2);
                QCOMPARE(out[0]->raw, message().chopped(1));
                QCOMPARE(out[1]->content(), QString("texte apres CRC"));
            }
        }
    }
    void independentCursorsAndOverflow() {
        EventQueue queue(3); auto event = std::make_shared<TextEvent>();
        for (int i = 0; i < 5; ++i) queue.push(event);
        auto first = queue.since(0); QCOMPARE(first.entries.size(), 3); QCOMPARE(first.lost, quint64(2));
        QCOMPARE(first.entries.first().sequence, quint64(3)); QCOMPARE(first.cursor, quint64(5));
        QCOMPARE(queue.since(0).entries.size(), 3); QVERIFY(queue.since(first.cursor).entries.isEmpty());
        queue.push(event); auto next = queue.since(first.cursor);
        QCOMPARE(next.entries.size(), 1); QCOMPARE(next.lost, quint64(0));
    }
    void codeBookFormatting() {
        QTemporaryFile file; QVERIFY(file.open());
        file.write("NOM;CODE;TEXTE\nA;1;valeur $.2V\nB;2;${KO|OK}\nC;3;$ fois\nD;4;$.2 / $.2 / $\nE;5;[$4.2][$4.0][$12.2]\n"); file.flush();
        CodeBook book; QVERIFY(book.load(file.fileName()));
        QCOMPARE(book.format(1, -123), QString("valeur -1.23V"));
        QCOMPARE(book.format(2, 0), QString("KO")); QCOMPARE(book.format(2, 1), QString("OK"));
        QCOMPARE(book.format(3, 42), QString("42 fois"));
        QCOMPARE(book.format(4, -123), QString("-1.23 / -1.23 / -123"));
        QCOMPARE(book.format(5, -123), QString("[  -1.23][-123][          -1.23]"));
        QCOMPARE(book.format(5, 123), QString("[   1.23][ 123][           1.23]"));
        QCOMPARE(book.format(5, 0), QString("[   0.00][   0][           0.00]"));
        QCOMPARE(book.format(5, -123456), QString("[-1234.56][-123456][       -1234.56]"));
        QVERIFY(book.format(254, 77).contains("77"));
    }
    void codeBookComments() {
        QTemporaryFile file; QVERIFY(file.open());
        file.write("# NOM;CODE;TEXTE\r\n"
                   "#IGNORE;1;ne doit pas remplacer le texte\r\n"
                   "  # IGNORE;2;autre commentaire\r\n"
                   "\r\n"
                   "A;1;valeur $.2V # commentaire;avec;colonnes\r\n"
                   "B;2;${KO|OK}# commentaire sans espace\r\n"
                   "C;3;texte;avec;separateurs # commentaire # suite\r\n"
                   "# A;1;remplacement interdit\r\n"
                   "D;4;fin sans retour ligne# commentaire");
        file.flush();
        CodeBook book; QVERIFY(book.load(file.fileName()));
        QCOMPARE(book.format(1, 123), QString("valeur 1.23V"));
        QCOMPARE(book.format(2, 0), QString("KO"));
        QCOMPARE(book.format(2, 1), QString("OK"));
        QCOMPARE(book.format(3, 0), QString("texte;avec;separateurs"));
        QCOMPARE(book.format(4, 0), QString("fin sans retour ligne"));
    }
    void quotedCodeBook() {
        QTemporaryFile file; QVERIFY(file.open());
        file.write("A;1;\"valeur $.2V\" # commentaire\n"
                   "B;2;\"texte #1; avec \"\"guillemets\"\"\"\n");
        QVERIFY(file.flush());
        CodeBook book; QVERIFY(book.load(file.fileName()));
        QCOMPARE(book.id("A"), 1);
        QCOMPARE(book.id("inconnu"), -1);
        QCOMPARE(book.format(1, 123), QString("valeur 1.23V"));
        QCOMPARE(book.format(2, 0), QString("texte #1; avec \"guillemets\""));
        const auto &settings = AppSettings::instance();
        QCOMPARE(settings.codeId("NONE"), 0);
        QCOMPARE(settings.codeId("INFO_ACCEL_SET"), 44);
        QCOMPARE(settings.codeId("INFO_ACCEL_GET"), 53);
        QCOMPARE(settings.codeId("INCONNU"), -1);
        QCOMPARE(settings.codes().format(settings.codeId("INFO_INIT_FIN"), 0), QString("== Initialisation terminée =="));
    }
    void boundedMeasurementHistory() {
        MeasurementStore store;
        for (int i = 0; i < 2100; ++i) {
            auto measure = std::make_shared<MeasureEvent>(); measure->time = quint32(i);
            auto accel = std::make_shared<AccelerometerEvent>(); accel->time = quint32(i);
            EventQueue::Batch batch;
            batch.entries = {{quint64(i * 2 + 1), measure}, {quint64(i * 2 + 2), accel}};
            store.append(batch);
        }
        QCOMPARE(store.measures.size(), size_t(2000));
        QCOMPARE(store.accelerometer.size(), size_t(400));
        QCOMPARE(static_cast<const MeasureEvent &>(*store.measures.front()).time, quint32(100));
        QCOMPARE(static_cast<const AccelerometerEvent &>(*store.accelerometer.front()).time, quint32(1700));
        QCOMPARE(static_cast<const MeasureEvent &>(*store.measures.back()).time, quint32(2099));
    }
    void concurrentProducer() {
        EventQueue queue(200); auto e = std::make_shared<TextEvent>();
        std::thread producer([&] { for (int i = 0; i < 10000; ++i) queue.push(e); });
        quint64 cursor = 0, seen = 0, lost = 0;
        for (int i = 0; i < 1000; ++i) {
            auto batch = queue.since(cursor); cursor = batch.cursor;
            seen += quint64(batch.entries.size()); lost += batch.lost;
        }
        producer.join(); auto final = queue.since(cursor); seen += quint64(final.entries.size()); lost += final.lost;
        QCOMPARE(seen + lost, quint64(10000));
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
