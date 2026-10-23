#include "serial/Commands.h"
#include "core/AppSettings.h"

QString Protocol::CommandRequest::text() const {
    const auto base = AppSettings::instance().command(command);
    return base.isEmpty() ? QString() : base + (values.isEmpty() ? QString() : ' ' + values.join(' '));
}

std::optional<Protocol::CommandRequest> Protocol::parseCommand(const QString &text) {
    const auto clean = text.simplified();
    const QString settingsPath;
    const auto &commands = AppSettings::instance(settingsPath).commands();
    std::optional<CommandRequest> result;
    qsizetype longest = 0;
    for (auto it = commands.cbegin(); it != commands.cend(); ++it) {
        const auto &base = it.value();
        if (base.size() > longest && (clean == base || clean.startsWith(base + ' '))) {
            const auto id = commandId(it.key());
            if (!id) continue;
            longest = base.size();
            result = CommandRequest{*id, clean.mid(base.size()).split(' ', Qt::SkipEmptyParts)};
        }
    }
    return result;
}
