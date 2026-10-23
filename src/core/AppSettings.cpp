#include "core/AppSettings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <limits>
#include <map>
#include <memory>
#include <mutex>

AppSettings::AppSettings(const QString &path) {
    // Use the default config.ini in the application directory if no path is provided.
    if (path.isEmpty()) {
        path_ = QDir(QCoreApplication::applicationDirPath()).filePath("config.ini");
    } else {
        path_ = path;
    }
}

QVariant AppSettings::defaultValue(const QString &key) {
    QSettings defaults(":/config.ini", QSettings::IniFormat);
    return defaults.value(key);
}

QVariant AppSettings::value(const QString &key) const {
    QSettings overrides(path_, QSettings::IniFormat);
    overrides.setFallbacksEnabled(false);
    const auto fallback = defaultValue(key);
    const auto result = overrides.value(key, fallback);
    // Invalid hand-edited values must not prevent a serial connection.
    if (key == "serial/baud") {
        bool ok = false;
        const int baud = result.toInt(&ok);
        return ok && baud > 0 ? QVariant(baud) : fallback;
    }
    if (key == "serial/automatic") {
        const auto text = result.toString().toLower();
        if (text == "true" || text == "1") return true;
        if (text == "false" || text == "0") return false;
        return fallback;
    }
    return result;
}

QString AppSettings::openFolder() const {
    const auto folder = value("files/openFolder").toString();
    if (folder.isEmpty()) return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir::isAbsolutePath(folder) ? folder
        : QDir(QFileInfo(path_).absolutePath()).absoluteFilePath(folder);
}

bool AppSettings::save(const QMap<QString, QVariant> &values, QString *error) const {
    QSettings settings(path_, QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    for (auto it = values.cbegin(); it != values.cend(); ++it) settings.setValue(it.key(), it.value());
    settings.sync();
    if (settings.status() == QSettings::NoError) return true;
    if (error) *error = QCoreApplication::translate("AppSettings", "Impossible d'enregistrer la configuration : %1").arg(path_);
    return false;
}

const AppSettings &AppSettings::instance(const QString &path) {
    const auto resolved = QFileInfo(path.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath("config.ini") : path).absoluteFilePath();
    static std::map<QString, std::unique_ptr<AppSettings>> instances;
    static std::mutex mutex;
    const std::lock_guard<std::mutex> lock(mutex);
    auto &settings = instances[resolved];
    if (!settings) settings.reset(new AppSettings(resolved));
    return *settings;
}

struct AppSettings::Resources {
    Resources();
    bool readStateFile(); // Charge le CSV et résout les références du diagramme JSON.
    bool readCodesFile();
    bool readCourbeFile();
    bool readDebugFile();
    bool readProtocolFile();

    QJsonObject states_, curves_;
    QMap<int, QString> stateNames_;
    QMap<QString, int> stateIds_;
    QMap<QString, QString> commands_;
    CodeBook codes_;
    DebugStyles debugStyles_;
    QString error_;
};

const AppSettings::Resources &AppSettings::resources() {
    static const Resources shared;
    return shared;
}

bool AppSettings::initialize(QString *error) {
    const auto &shared = resources();
    if (error) *error = shared.error_;
    return shared.error_.isEmpty();
}

const QJsonObject &AppSettings::states() const { return resources().states_; }
const QJsonObject &AppSettings::curves() const { return resources().curves_; }
const CodeBook &AppSettings::codes() const { return resources().codes_; }
int AppSettings::codeId(const QString &name) const { return resources().codes_.id(name); }
QString AppSettings::command(const QString &name) const { return resources().commands_.value(name); }
QString AppSettings::command(Protocol::CMD command) const { return this->command(Protocol::commandName(command)); }
const QMap<QString, QString> &AppSettings::commands() const { return resources().commands_; }
const DebugStyles &AppSettings::debugStyles() const { return resources().debugStyles_; }
QString AppSettings::stateName(qint32 number) const { return resources().stateNames_.value(number, QStringLiteral("État inconnu"));}
int AppSettings::stateId(const QString &name) const { return resources().stateIds_.value(name, -1);}

QJsonObject AppSettings::readObject(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QString("%1 : %2").arg(path, file.errorString());
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QString("%1 : document JSON invalide (%2)").arg(path, parseError.errorString());
        return {};
    }
    return document.object();
}

AppSettings::Resources::Resources() {
    if (!readProtocolFile()) return;
    if (!readStateFile()) return;
    if (!readCodesFile()) return;
    if (!readCourbeFile()) return;
    if (!readDebugFile()) return;
}

bool AppSettings::Resources::readProtocolFile() {
    QFile file(":/protocol.json");
    if (!file.open(QIODevice::ReadOnly)) {
        error_ = "Impossible de charger protocol.json : " + file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray() || document.array().isEmpty()) {
        error_ = "protocol.json : liste de commandes invalide ou vide";
        return false;
    }
    const auto commands = document.array();
    const QRegularExpression identifier("^[A-Za-z_][A-Za-z0-9_]*$");
    QSet<QString> texts;
    for (const auto &entry : commands) {
        const auto object = entry.toObject();
        const auto key = object.value("key").toString();
        const auto text = object.value("cmd").toString();
        if (!identifier.match(key).hasMatch() || commands_.contains(key)
            || !object.value("cmd").isString() || text.isEmpty() || text != text.simplified()
            || text.contains('\n') || text.contains('\r') || text.contains('\t') || texts.contains(text)) {
            error_ = "protocol.json : commande invalide ou dupliquee : " + key;
            return false;
        }
        commands_.insert(key, text);
        texts.insert(text);
    }
    return true;
}

bool AppSettings::Resources::readStateFile() {
    QFile csv(":/etats.csv");
    if (!csv.open(QIODevice::ReadOnly)) {
        error_ = "Impossible de charger etats.csv";
        return false;
    }
    auto &stateCodes = stateIds_;
    const QRegularExpression identifier("^[A-Z_][A-Z0-9_]*$");
    int lineNumber = 0;
    while (!csv.atEnd()) {
        const auto line = QString::fromUtf8(csv.readLine()).trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith('#')) continue;
        auto fields = line.split(';');
        for (auto &field : fields) field = field.trimmed();
        bool ok = false;
        const auto number = fields.value(2).toLongLong(&ok);
        const auto key = fields.value(0) + '_' + fields.value(1);
        if (fields.size() != 4 || !identifier.match(fields.value(0)).hasMatch()
            || !identifier.match(fields.value(1)).hasMatch() || !ok || number < 0
            || number > std::numeric_limits<qint32>::max() || fields.value(3).isEmpty()
            || stateCodes.contains(key) || stateNames_.contains(int(number))) {
            error_ = QString("etats.csv : ligne %1 invalide ou dupliquee").arg(lineNumber);
            return false;
        }
        stateCodes.insert(key, int(number));
        stateNames_.insert(int(number), fields[3]);
    }
    if (stateCodes.isEmpty()) {
        error_ = "etats.csv : aucun etat defini";
        return false;
    }
    states_ = readObject(":/machines.json", &error_);
    if (!error_.isEmpty()) return false;
    if (!states_.value("machines").isArray() || states_.value("machines").toArray().isEmpty()) {
        error_ = "machines.json : aucune machine definie";
        return false;
    }
    // Resolve the editable references once; views consume normalized wire codes.
    QJsonArray machines;
    for (const auto &entry : states_.value("machines").toArray()) {
        auto machine = entry.toObject();
        const auto prefix = machine.value("type").toString() + '_';
        QMap<int, int> ids;
        QSet<int> codes;
        QJsonArray states;
        for (const auto &value : machine.value("etats").toArray()) {
            auto state = value.toObject();
            const auto key = prefix + state.value("etat").toString();
            const auto idValue = state.value("id");
            const int id = idValue.toInt(-1);
            if (!stateCodes.contains(key) || !idValue.isDouble() || id < 0
                || idValue.toDouble() != id || ids.contains(id) || codes.contains(stateCodes.value(key))) {
                error_ = "machines.json : reference ou id invalide pour " + key;
                return false;
            }
            const int code = stateCodes.value(key);
            ids.insert(id, code);
            codes.insert(code);
            state.insert("code", code);
            state.insert("nom", stateNames_.value(code));
            if (state.contains("cmd") && (!state.value("cmd").isString()
                || state.value("cmd").toString().contains('\n')
                || state.value("cmd").toString().contains('\r'))) {
                error_ = "machines.json : commande brute invalide pour " + key;
                return false;
            }
            states.append(state);
        }
        QJsonArray transitions;
        for (const auto &value : machine.value("transitions").toArray()) {
            auto transition = value.toObject();
            for (const auto *endpoint : {"start", "end"}) {
                const auto reference = transition.value(endpoint);
                const int id = reference.toInt(-1);
                if (!reference.isDouble() || reference.toDouble() != id || !ids.contains(id)) {
                    error_ = QString("machines.json : transition %1 inconnue").arg(endpoint);
                    return false;
                }
                transition.insert(endpoint, ids.value(id));
            }
            transitions.append(transition);
        }
        if (machine.contains("default_state")) {
            const auto key = prefix + machine.value("default_state").toString();
            if (!stateCodes.contains(key) || !codes.contains(stateCodes.value(key))) {
                error_ = "machines.json : etat par defaut inconnu : " + key;
                return false;
            }
            machine.insert("default_state", stateCodes.value(key));
        }
        if (machine.contains("display_when")) {
            auto condition = machine.value("display_when").toObject();
            const auto key = condition.value("state").toString();
            if (!stateCodes.contains(key)) {
                error_ = "machines.json : condition inconnue : " + key;
                return false;
            }
            condition.insert("state", stateCodes.value(key));
            machine.insert("display_when", condition);
        }
        machine.insert("etats", states);
        machine.insert("transitions", transitions);
        machines.append(machine);
    }
    states_.insert("machines", machines);
    return true;
}

bool AppSettings::Resources::readCodesFile() {
    if (!codes_.load(":/codes.csv")) {
        error_ = "Impossible de charger codes.csv";
        return false;
    }
    return true;
}

bool AppSettings::Resources::readCourbeFile() {
    curves_ = readObject(":/courbe.json", &error_);
    if (error_.isEmpty() && (!curves_.value("courbes").isArray() || curves_.value("courbes").toArray().isEmpty()))
        error_ = "courbe.json : aucune courbe definie";
    return error_.isEmpty();
}

bool AppSettings::Resources::readDebugFile() {
    const auto debug = readObject(":/debug.json", &error_);
    if (!error_.isEmpty()) return false;
    return debugStyles_.load(debug, &error_);
}
