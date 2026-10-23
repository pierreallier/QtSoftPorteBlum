#pragma once
#include "core/Event.h"
#include <QVector>
// Décode les fragments reçus ; aucune supposition sur les frontières de readAll().
class SerialDecoder {
    public:
        SerialDecoder() = default;
        QVector<EventPtr> feed(const QByteArray &bytes);
        void reset() { buffer_.clear(); }
    private:
        QByteArray buffer_;
};
