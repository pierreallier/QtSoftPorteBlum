#include "core/Event.h"
#include "core/AppSettings.h"
#include <QFile>
#include <QStringList>
#include <QRegularExpression>
#include <cmath>


// Substitution numérique commune : valeurs brutes ou valeur répétée d'un code.
namespace {
QString formatValues(const QString &pattern, const QVector<qint64> &values, bool repeat) {
    static const QRegularExpression token(R"(\$\{([^|}]+)\|([^}]+)\}|\$([0-9]*)\.([0-6])|\$)");
    QString result;
    qsizetype end = 0, index = 0;
    auto matches = token.globalMatch(pattern);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += pattern.mid(end, match.capturedStart() - end);
        if (values.isEmpty() || (!repeat && index >= values.size())) {
            result += match.captured();
        } else {
            const auto value = values[repeat ? 0 : index++];
            if (!match.captured(1).isEmpty()) result += match.captured(value == 0 ? 1 : 2);
            else if (!match.captured(4).isEmpty()) {
                const int width = match.captured(3).toInt();
                const int decimals = match.captured(4).toInt();
                const auto number = QString::number(value / std::pow(10.0, decimals), 'f', decimals);
                result += number.rightJustified(width + (decimals > 0 ? decimals + 1 : 0), ' ');
            } else result += QString::number(value);
        }
        end = match.capturedEnd();
    }
    return result + pattern.mid(end);
}
} // namespace

QString StateEvent::content() const { return AppSettings::instance().stateName(value); }
QString MessageEvent::content() const { return AppSettings::instance().codes().format(code, value); }

namespace {
    template <size_t N>
    QStringList integerValues(const std::array<qint64, N> &values) {
        QStringList result;
        for (auto value : values) result.append(QString::number(value));
        return result;
    }
}
QString MeasureEvent::content() const { return integerValues(rawValues()).join(' '); }
QString AccelerometerEvent::content() const { return integerValues(rawValues()).join(' '); }



bool CodeBook::load(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return false;
    texts_.clear();
    ids_.clear();
    for (auto line : QString::fromUtf8(f.readAll()).split('\n')) {
        bool quoted = false;
        for (qsizetype i = 0; i < line.size(); ++i) {
            if (line[i] == '"') {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"') ++i;
                else quoted = !quoted;
            } else if (line[i] == '#' && !quoted) {
                line.truncate(i);
                break;
            }
        }
        const auto fields = line.trimmed().split(';');
        if (fields.size() < 3) continue;
        bool ok = false; int code = fields[1].trimmed().toInt(&ok);
        const auto name = fields[0].trimmed();
        if (!ok || code < 0 || code > 255 || name.isEmpty() || quoted) continue;
        auto text = fields.mid(2).join(';').trimmed();
        if (text.size() >= 2 && text.startsWith('"') && text.endsWith('"')) {
            text = text.mid(1, text.size() - 2);
            text.replace("\"\"", "\"");
        }
        texts_.insert(code, text);
        ids_.insert(name, code);
    }
    return !texts_.isEmpty();
}

int CodeBook::id(const QString &name) const { return ids_.value(name, -1); }

QString CodeBook::format(quint8 code, qint32 value) const {
    return formatValues(texts_.value(code, QString("Code inconnu %1 ; valeur=%2").arg(code).arg(value)), {value}, true);
}
