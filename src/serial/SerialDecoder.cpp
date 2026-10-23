#include "serial/SerialDecoder.h"
#include <QtEndian>

namespace {
    quint8 crc8(const QByteArray &data, qsizetype size) {
        quint8 crc = 0;
        for (qsizetype i = 0; i < size; ++i) {
            crc ^= static_cast<quint8>(data[i]);
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc & 0x80) ? quint8((crc << 1) ^ 0x07) : quint8(crc << 1);
        }
        return crc;
    }
    template<class T> T take(const char *&p) {
        T v = qFromLittleEndian<T>(p);
        p += sizeof(T);
        return v;
    }
}

QVector<EventPtr> SerialDecoder::feed(const QByteArray &bytes) {
    buffer_.append(bytes); QVector<EventPtr> result;
    while (!buffer_.isEmpty()) {
        qsizetype size;
        if (buffer_[0] != '\1') { // Texte UTF-8, jusqu'à la fin de ligne.
            const auto nl = buffer_.indexOf('\n');
            if (nl < 0) break;
            size = nl + 1;
            if (buffer_.first(size).trimmed().isEmpty()) {
                buffer_.remove(0, size);
                continue;
            }
            auto e = std::make_shared<TextEvent>(); e->raw = buffer_.first(size);
            e->line = QString::fromUtf8(e->raw).trimmed(); result.push_back(e);
        } else { // Trame binaire, avec marqueur
            if (buffer_.size() < 2) break;
            const char tag = buffer_[1];
            // Tailles incluant le marqueur, le tag et le CRC-8/SMBUS.
            size = (tag == 'M') ? 27 : ((tag == 'A') ? 25 : 8);
            if (buffer_.size() < size) break;
            if (crc8(buffer_, size - 1) != static_cast<quint8>(buffer_[size - 1])) {
                buffer_.remove(0, size);
                continue;
            }
            const char *p = buffer_.constData() + 2;
            if (tag != 'M' && tag != 'A') { // Message, Request ou State
                std::shared_ptr<MessageEvent> e;
                if (tag == 'S') e = std::make_shared<StateEvent>();
                else if (tag == 'E') e = std::make_shared<ErrorEvent>();
                else if (tag == 'W') e = std::make_shared<WarningEvent>();
                else e = std::make_shared<MessageEvent>();
                e->type = tag;
                e->code = take<quint8>(p); e->value = take<qint32>(p);
                e->raw = buffer_.first(size); result.push_back(e);
            } else if (tag == 'M') { // Mesure
                auto e = std::make_shared<MeasureEvent>(); e->time = take<quint32>(p);
                e->voltage = take<qint16>(p); e->current = take<qint16>(p);
                e->motorAngle = take<qint32>(p); e->motorSpeed = take<qint32>(p);
                e->doorAngle = take<qint32>(p); e->target = take<qint32>(p);
                e->raw = buffer_.first(size); result.push_back(e);
            } else { // tag=='A' Accéléromètre
                auto e = std::make_shared<AccelerometerEvent>(); e->time = take<quint32>(p);
                for (auto &v : e->acceleration) v = take<qint16>(p);
                for (auto &v : e->velocity) v = take<qint16>(p);
                for (auto &v : e->angles) v = take<qint16>(p);
                e->raw = buffer_.first(size); result.push_back(e);
            }
        }
        buffer_.remove(0, size);
    }
    return result;
}
