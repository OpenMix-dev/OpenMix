#include "AllenHeathTcpProtocol.h"
#include "../../core/Cue.h"
#include <QtEndian>
#include <cstring>

namespace OpenMix {

// ACE protocol message types
namespace AceMessage {
constexpr quint8 TYPE_KEEPALIVE = 0x01;
constexpr quint8 TYPE_PARAMETER = 0x02;
constexpr quint8 TYPE_SCENE = 0x03;
constexpr quint8 TYPE_DCA = 0x10;
} // namespace AceMessage

AllenHeathTcpProtocol::AllenHeathTcpProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort; // 51321

    QObject::connect(&m_transport, &TcpTransport::connected, this,
                     &AllenHeathTcpProtocol::onTransportConnected);
    QObject::connect(&m_transport, &TcpTransport::disconnected, this,
                     &AllenHeathTcpProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &TcpTransport::connectionError, this,
                     &AllenHeathTcpProtocol::onTransportError);
    QObject::connect(&m_transport, &TcpTransport::connectionLost, this,
                     &AllenHeathTcpProtocol::onTransportConnectionLost);
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     &AllenHeathTcpProtocol::onDataReceived);
    QObject::connect(&m_transport, &TcpTransport::reconnecting, this,
                     &AllenHeathTcpProtocol::onReconnecting);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &AllenHeathTcpProtocol::onKeepAliveTimeout);
}

AllenHeathTcpProtocol::~AllenHeathTcpProtocol() { disconnect(); }

bool AllenHeathTcpProtocol::connect(const QString& host, int port) {
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(port));

    return m_transport.connect(host, port);
}

void AllenHeathTcpProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_transport.disconnect();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathTcpProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    if (path.startsWith("/dca/")) {
        QStringList parts = path.split('/');
        if (parts.size() >= 4) {
            bool ok;
            int dca = parts[2].toInt(&ok);
            if (!ok || dca < 1)
                return;
            QString param = parts[3];

            if (param == "fader") {
                QByteArray msg = buildDCAFaderMessage(dca, value.toFloat());
                m_transport.send(msg);
            } else if (param == "mute") {
                QByteArray msg = buildDCAMuteMessage(dca, value.toBool());
                m_transport.send(msg);
            }
        }
    }

    m_parameterCache[path] = value;
}

QVariant AllenHeathTcpProtocol::getParameter(const QString& path) {
    return m_parameterCache.value(path);
}

void AllenHeathTcpProtocol::requestParameter(const QString& path) {
    // binary protocol, params pushed by console
    Q_UNUSED(path);
}

void AllenHeathTcpProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_parameterCache.contains(path)) {
        if (callback) {
            callback(path, m_parameterCache[path], true);
        }
    } else {
        if (callback) {
            callback(path, QVariant(), false);
        }
    }
}

void AllenHeathTcpProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void AllenHeathTcpProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QByteArray msg = buildSceneRecallMessage(sceneNumber);
    m_transport.send(msg);
}

void AllenHeathTcpProtocol::refresh() {
    // binary protocol doesn't support general refresh
}

QByteArray AllenHeathTcpProtocol::buildDCAFaderMessage(int dca, float level) {
    if (dca < 1)
        return {};

    QByteArray msg;

    // ACE Protocol frame: [Length(2)] [Type(1)] [Payload...]
    // DCA fader: Type 0x10, Payload: [DCA index(1)] [Level(4, float BE)]

    quint16 length = 6; // 1 byte type + 1 byte DCA + 4 bytes float
    msg.append(static_cast<char>((length >> 8) & 0xFF));
    msg.append(static_cast<char>(length & 0xFF));
    msg.append(static_cast<char>(AceMessage::TYPE_DCA));
    msg.append(static_cast<char>(dca - 1)); // 0-indexed

    // float in big-endian
    float clamped = qBound(0.0f, level, 1.0f);
    quint32 i;
    std::memcpy(&i, &clamped, 4);
    quint32 be = qToBigEndian(i);
    msg.append(reinterpret_cast<const char*>(&be), 4);

    return msg;
}

QByteArray AllenHeathTcpProtocol::buildDCAMuteMessage(int dca, bool muted) {
    if (dca < 1)
        return {};

    QByteArray msg;

    // DCA mute: Type 0x10, Payload: [DCA index(1)] [0x80 flag for mute] [Mute state(1)]
    quint16 length = 4;
    msg.append(static_cast<char>((length >> 8) & 0xFF));
    msg.append(static_cast<char>(length & 0xFF));
    msg.append(static_cast<char>(AceMessage::TYPE_DCA));
    msg.append(static_cast<char>((dca - 1) | 0x80)); // 0x80 indicates mute operation
    msg.append(static_cast<char>(muted ? 1 : 0));

    return msg;
}

QByteArray AllenHeathTcpProtocol::buildSceneRecallMessage(int sceneNumber) {
    QByteArray msg;

    // scene recall: Type 0x03, Payload: [Scene number(2, BE)]
    quint16 length = 3;
    msg.append(static_cast<char>((length >> 8) & 0xFF));
    msg.append(static_cast<char>(length & 0xFF));
    msg.append(static_cast<char>(AceMessage::TYPE_SCENE));
    msg.append(static_cast<char>((sceneNumber >> 8) & 0xFF));
    msg.append(static_cast<char>(sceneNumber & 0xFF));

    return msg;
}

QByteArray AllenHeathTcpProtocol::buildKeepAliveMessage() {
    QByteArray msg;

    quint16 length = 1;
    msg.append(static_cast<char>((length >> 8) & 0xFF));
    msg.append(static_cast<char>(length & 0xFF));
    msg.append(static_cast<char>(AceMessage::TYPE_KEEPALIVE));

    return msg;
}

void AllenHeathTcpProtocol::parseProtocolData(const QByteArray& data) {
    m_receiveBuffer.append(data);

    while (m_receiveBuffer.size() >= 3) {
        quint16 length = (static_cast<quint8>(m_receiveBuffer[0]) << 8) |
                         static_cast<quint8>(m_receiveBuffer[1]);

        if (m_receiveBuffer.size() < length + 2)
            break;

        QByteArray frame = m_receiveBuffer.mid(2, length);
        m_receiveBuffer.remove(0, length + 2);

        if (frame.isEmpty())
            continue;

        quint8 type = static_cast<quint8>(frame[0]);

        switch (type) {
        case AceMessage::TYPE_KEEPALIVE:
            // could be used to measure round-trip time if we track send time
            break;

        case AceMessage::TYPE_PARAMETER:
            if (frame.size() >= 3) {
                int pathLen = static_cast<quint8>(frame[1]);
                if (frame.size() >= 2 + pathLen + 2) {
                    QString path = QString::fromUtf8(frame.mid(2, pathLen));
                    quint8 valueType = static_cast<quint8>(frame[2 + pathLen]);

                    QVariant value;
                    int valueOffset = 3 + pathLen;

                    if (valueType == 0x01 && frame.size() >= valueOffset + 4) {
                        // float value
                        quint32 be;
                        std::memcpy(&be, frame.constData() + valueOffset, 4);
                        quint32 hostBits = qFromBigEndian(be);
                        float f;
                        std::memcpy(&f, &hostBits, 4);
                        value = f;
                    } else if (valueType == 0x02 && frame.size() >= valueOffset + 4) {
                        quint32 be;
                        memcpy(&be, frame.constData() + valueOffset, 4);
                        value = static_cast<int>(qFromBigEndian(be));
                    } else if (valueType == 0x03 && frame.size() >= valueOffset + 1) {
                        value = frame[valueOffset] != 0;
                    }

                    if (value.isValid()) {
                        m_parameterCache[path] = value;
                        emit parameterChanged(path, value);
                    }
                }
            }
            break;

        case AceMessage::TYPE_SCENE:
            if (frame.size() >= 3) {
                int sceneNumber =
                    (static_cast<quint8>(frame[1]) << 8) | static_cast<quint8>(frame[2]);
                emit sceneChanged(sceneNumber);
            }
            break;

        case AceMessage::TYPE_DCA:
            if (frame.size() >= 2) {
                int dcaIndex = static_cast<quint8>(frame[1]);
                bool isMute = (dcaIndex & 0x80) != 0;
                dcaIndex &= 0x7F;

                if (isMute && frame.size() >= 3) {
                    bool muted = frame[2] != 0;
                    QString path = QString("/dca/%1/mute").arg(dcaIndex + 1);
                    m_parameterCache[path] = muted;
                    emit parameterChanged(path, muted);
                } else if (frame.size() >= 6) {
                    quint32 be;
                    std::memcpy(&be, frame.constData() + 2, 4);
                    quint32 hostBits = qFromBigEndian(be);
                    float f;
                    std::memcpy(&f, &hostBits, 4);
                    QString path = QString("/dca/%1/fader").arg(dcaIndex + 1);
                    m_parameterCache[path] = f;
                    emit parameterChanged(path, f);
                }
            }
            break;

        default:
            break;
        }
    }
}

void AllenHeathTcpProtocol::onTransportConnected() {
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

    m_keepAliveTimer.start(KEEPALIVE_INTERVAL);

    emit connected();
}

void AllenHeathTcpProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathTcpProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);
}

void AllenHeathTcpProtocol::onTransportConnectionLost() {
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void AllenHeathTcpProtocol::onDataReceived(const QByteArray& data) { parseProtocolData(data); }

void AllenHeathTcpProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        m_transport.send(buildKeepAliveMessage());
    }
}

void AllenHeathTcpProtocol::onReconnecting(int attempt, int maxAttempts) {
    setStatus(QString("Reconnecting (attempt %1/%2)...").arg(attempt).arg(maxAttempts));
}

void AllenHeathTcpProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void AllenHeathTcpProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
