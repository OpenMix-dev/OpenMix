#include "OscTransport.h"
#include <QNetworkDatagram>
#include <cstdlib>
#include <cstring>

namespace OpenMix {

OscTransport::OscTransport(QObject* parent) : QObject(parent) {
    QObject::connect(&m_socket, &QUdpSocket::readyRead, this, &OscTransport::onReadyRead);
}

OscTransport::~OscTransport() { disconnect(); }

bool OscTransport::connect(const QString& host, int port) {
    if (m_connected) {
        disconnect();
    }

    m_host = host;
    m_port = port;

    m_target = QHostAddress(host);
    bool isIPv4 = false;
    const quint32 v4 = m_target.toIPv4Address(&isIPv4);
    if (isIPv4) {
        m_target = QHostAddress(v4);
    }
    if (m_target.isNull()) {
        emit connectionError("Invalid mixer address");
        return false;
    }

    if (!m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        emit connectionError("Failed to bind UDP socket");
        return false;
    }

    m_connected = true;
    emit connected();
    return true;
}

void OscTransport::disconnect() {
    m_socket.close();
    m_connected = false;
    emit disconnected();
}

void OscTransport::send(const QString& path) {
    lo_message msg = lo_message_new();
    sendMessage(path, msg);
    lo_message_free(msg);
}

void OscTransport::send(const QString& path, float value) {
    lo_message msg = lo_message_new();
    lo_message_add_float(msg, value);
    sendMessage(path, msg);
    lo_message_free(msg);
}

void OscTransport::send(const QString& path, int value) {
    lo_message msg = lo_message_new();
    lo_message_add_int32(msg, value);
    sendMessage(path, msg);
    lo_message_free(msg);
}

void OscTransport::send(const QString& path, const QString& value) {
    lo_message msg = lo_message_new();
    lo_message_add_string(msg, value.toUtf8().constData());
    sendMessage(path, msg);
    lo_message_free(msg);
}

void OscTransport::sendMessage(const QString& path, lo_message msg) {
    if (!m_connected || !msg)
        return;

    const QByteArray pathBytes = path.toUtf8();
    size_t length = 0;
    void* buffer = lo_message_serialise(msg, pathBytes.constData(), nullptr, &length);
    if (!buffer)
        return;

    m_socket.writeDatagram(static_cast<const char*>(buffer), static_cast<qint64>(length), m_target,
                           static_cast<quint16>(m_port));
    std::free(buffer);
}

void OscTransport::send(const QString& path, const QVariant& value) {
    if (!m_connected)
        return;

    switch (value.typeId()) {
    case QMetaType::Float:
    case QMetaType::Double:
        send(path, value.toFloat());
        break;
    case QMetaType::Int:
    case QMetaType::Bool:
        send(path, value.toInt());
        break;
    case QMetaType::QString:
        send(path, value.toString());
        break;
    default:
        // try float as default
        send(path, value.toFloat());
        break;
    }
}

void OscTransport::onReadyRead() {
    while (m_socket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket.receiveDatagram();
        QByteArray data = datagram.data();

        emit rawMessageReceived(data);
        parseOscMessage(data);
    }
}

void OscTransport::parseOscMessage(const QByteArray& data) {
    // OSC format: path (null-padded to 4-byte boundary), type tag string, arguments

    if (data.size() < 4)
        return;

    // extract path
    int pathEnd = data.indexOf('\0');
    if (pathEnd < 0)
        return;
    QString path = QString::fromUtf8(data.left(pathEnd));

    // skip to type tag (4-byte aligned)
    int typeStart = ((pathEnd + 4) / 4) * 4;
    if (typeStart >= data.size()) {
        // message w/ no arguments
        emit messageReceived(path, QVariant());
        return;
    }

    // find type tag
    if (data.at(typeStart) != ',')
        return;
    int typeEnd = data.indexOf('\0', typeStart);
    if (typeEnd < 0)
        return;
    QString types = QString::fromUtf8(data.mid(typeStart + 1, typeEnd - typeStart - 1));

    // skip to arguments (4-byte aligned)
    int argOffset = ((typeEnd + 4) / 4) * 4;
    if (argOffset > data.size())
        return;

    if (!types.isEmpty()) {
        QVariant value = parseOscArgument(data, argOffset, types.at(0).toLatin1());
        emit messageReceived(path, value);
    }
}

QVariant OscTransport::parseOscArgument(const QByteArray& data, int& offset, char type) {
    switch (type) {
    case 'f': {
        if (offset + 4 <= data.size()) {
            // big-endian float
            quint32 raw = (static_cast<quint8>(data[offset]) << 24) |
                          (static_cast<quint8>(data[offset + 1]) << 16) |
                          (static_cast<quint8>(data[offset + 2]) << 8) |
                          static_cast<quint8>(data[offset + 3]);
            float f;
            std::memcpy(&f, &raw, sizeof(float));
            offset += 4;
            return f;
        }
        break;
    }
    case 'i': {
        if (offset + 4 <= data.size()) {
            qint32 i = (static_cast<quint8>(data[offset]) << 24) |
                       (static_cast<quint8>(data[offset + 1]) << 16) |
                       (static_cast<quint8>(data[offset + 2]) << 8) |
                       static_cast<quint8>(data[offset + 3]);
            offset += 4;
            return i;
        }
        break;
    }
    case 's': {
        int strEnd = data.indexOf('\0', offset);
        if (strEnd > offset) {
            QString str = QString::fromUtf8(data.mid(offset, strEnd - offset));
            // advance offset to next 4-byte boundary
            offset = ((strEnd + 4) / 4) * 4;
            return str;
        }
        break;
    }
    case 'b': {
        // blob: 4-byte size + data + padding
        if (offset + 4 <= data.size()) {
            qint32 size = (static_cast<quint8>(data[offset]) << 24) |
                          (static_cast<quint8>(data[offset + 1]) << 16) |
                          (static_cast<quint8>(data[offset + 2]) << 8) |
                          static_cast<quint8>(data[offset + 3]);
            if (size < 0)
                break;
            offset += 4;
            if (offset + size <= data.size()) {
                QByteArray blob = data.mid(offset, size);
                offset += ((size + 3) / 4) * 4; // pad to 4-byte boundary
                return blob;
            }
        }
        break;
    }
    }

    return QVariant();
}

} // namespace OpenMix
