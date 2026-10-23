#include "core/DebugStyles.h"
#include <QJsonArray>
#include <QColor>

namespace {
QColor parseColor(const QString &text) {
    // JSON uses CSS notation: #RRGGBB or #RRGGBBAA (alpha last).
    if (text.startsWith('#') && text.size() == 9) {
        bool valid = false;
        const auto rgba = text.mid(1).toUInt(&valid, 16);
        if (!valid) return {};
        return QColor((rgba >> 24) & 255, (rgba >> 16) & 255, (rgba >> 8) & 255, rgba & 255);
    }
    return QColor(text);
}
}

bool DebugStyles::load(const QJsonObject &root, QString *error) {
    if (error) error->clear();
    const auto fail = [error](const QString &reason) {
        if (error) *error = QStringLiteral("debug.json : ") + reason;
        return false;
    };
    if (!root.value("types").isArray() || root.value("types").toArray().isEmpty())
        return fail(QStringLiteral("la liste types doit contenir au moins un format"));
    QHash<char, DebugStyle> styles;
    bool prefixes = false;
    for (const auto &entry : root.value("types").toArray()) {
        if (!entry.isObject()) return fail(QStringLiteral("chaque format doit être un objet"));
        const auto object = entry.toObject();
        const auto type = object.value("type").toString();
        if (type.size() != 1 || type.front().unicode() > 127 || !type.front().isPrint() || type.front().isSpace())
            return fail(QStringLiteral("type doit contenir un caractère ASCII imprimable"));
        const char key = type.front().toLatin1();
        if (styles.contains(key)) return fail(QStringLiteral("type dupliqué : %1").arg(type));
        DebugStyle style;
        for (const auto &field : {QStringLiteral("prefix"), QStringLiteral("message")}) {
            const auto value = object.value(field);
            if (!value.isUndefined() && !value.isNull() && !value.isString())
                return fail(QStringLiteral("%1.%2 doit être une chaîne ou null").arg(type, field));
        }
        style.prefix = object.value("prefix").toString();
        const auto prefixCharacters = style.prefix.toUcs4();
        if (prefixCharacters.size() > 1 || (!prefixCharacters.isEmpty() && !QChar::isPrint(prefixCharacters.first())))
            return fail(QStringLiteral("%1.prefix est limité à un seul caractère imprimable").arg(type));
        style.message = object.value("message").toString("$");
        const auto alignment = object.value("justify");
        if (!alignment.isUndefined() && !alignment.isNull()) {
            if (!alignment.isString()) return fail(QStringLiteral("%1.justify doit être une chaîne ou null").arg(type));
            const auto value = alignment.toString();
            if (value == "left") style.alignment = Qt::AlignLeft;
            else if (value == "right") style.alignment = Qt::AlignRight;
            else if (value == "center") style.alignment = Qt::AlignHCenter;
            else if (value == "justify") style.alignment = Qt::AlignJustify;
            else return fail(QStringLiteral("%1.justify : left, right, center ou justify attendu").arg(type));
        }
        for (const auto &field : {QStringLiteral("foreground"), QStringLiteral("background")}) {
            const auto value = object.value(field);
            if (value.isUndefined() || value.isNull()) continue;
            const auto color = value.isString() ? parseColor(value.toString()) : QColor();
            if (!color.isValid()) return fail(QStringLiteral("%1.%2 : couleur invalide").arg(type, field));
            if (field == "foreground") style.characters.setForeground(color);
            else style.characters.setBackground(color);
        }
        for (const auto &field : {QStringLiteral("underline"), QStringLiteral("overstrike")}) {
            const auto value = object.value(field);
            if (value.isUndefined() || value.isNull()) continue;
            if (!value.isBool()) return fail(QStringLiteral("%1.%2 doit être un booléen ou null").arg(type, field));
            if (field == "underline") style.characters.setFontUnderline(value.toBool());
            else style.characters.setFontStrikeOut(value.toBool());
        }
        prefixes = prefixes || !style.prefix.isEmpty();
        styles.insert(key, style);
    }
    styles_ = std::move(styles);
    hasPrefixes_ = prefixes;
    return true;
}

const DebugStyle &DebugStyles::style(char type) const {
    const auto found = styles_.constFind(type);
    return found == styles_.cend() ? fallback_ : found.value();
}
