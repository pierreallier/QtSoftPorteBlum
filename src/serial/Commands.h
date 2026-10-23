#pragma once
#include "serial/CommandIds.h"
#include <QString>
#include <QStringList>
#include <QMetaType>
#include <optional>

namespace Protocol {
struct CommandRequest {
    CMD command; // Identifiant généré depuis les clés de protocol.json.
    QStringList values;
    QString text() const;
};
std::optional<CommandRequest> parseCommand(const QString &text);
}
Q_DECLARE_METATYPE(Protocol::CommandRequest)
