#include "DiGiCoProtocol.h"
#include "../../core/Cue.h"
#include <QtEndian>
#include <cmath>

namespace OpenMix {

namespace {

// MMIX "set" command opcode (fader/mute/level writes).
const QByteArray kOpcodeSet = QByteArray::fromHex("01110108");
// MMIX subscription opcode (attach to the "Mixing" object tree).
const QByteArray kOpcodeSubscribe = QByteArray::fromHex("01010102");
// EEVT client-registration opcode. Recovered strings are exact; the opcode word
// itself was not cleanly recoverable, so this is a best-effort value.
const QByteArray kOpcodeEvent = QByteArray::fromHex("01010102");

// centidB limits: fully-off fader sentinel and gain/EQ clamp seen on the wire.
constexpr qint32 kFaderOff = -15000;   // -150.00 dB
constexpr qint32 kFaderMax = 1000;     // +10.00 dB

void appendBE32(QByteArray& out, quint32 value) {
    out.append(static_cast<char>((value >> 24) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

// Two hex digits of the zero-based object index, as the reference formats it.
QString objectIndex(int oneBased) {
    return QString("%1").arg(oneBased - 1, 2, 16, QChar('0'));
}

} // namespace

DiGiCoProtocol::DiGiCoProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort; // 51321

    QObject::connect(&m_transport, &TcpTransport::connected, this,
                     &DiGiCoProtocol::onTransportConnected);
    QObject::connect(&m_transport, &TcpTransport::disconnected, this,
                     &DiGiCoProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &TcpTransport::connectionError, this,
                     &DiGiCoProtocol::onTransportError);
    QObject::connect(&m_transport, &TcpTransport::connectionLost, this,
                     &DiGiCoProtocol::onTransportConnectionLost);
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     &DiGiCoProtocol::onDataReceived);
    QObject::connect(&m_transport, &TcpTransport::reconnecting, this,
                     &DiGiCoProtocol::onReconnecting);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &DiGiCoProtocol::onKeepAliveTimeout);
}

DiGiCoProtocol::~DiGiCoProtocol() { disconnect(); }

// --- frame construction ----------------------------------------------------

QByteArray DiGiCoProtocol::buildFrame(const char* tag, const QByteArray& opcode,
                                      const QByteArray& elements) {
    QByteArray payload = opcode + elements;

    QByteArray afterLen;
    afterLen.append('\x11'); // fixed marker
    afterLen.append('\0');
    afterLen.append('\0');
    afterLen.append('\0');
    afterLen.append(static_cast<char>(payload.size() & 0xFF)); // inner length
    afterLen += payload;

    QByteArray frame;
    frame.append(tag, 4);
    appendBE32(frame, static_cast<quint32>(afterLen.size()));
    frame += afterLen;
    return frame;
}

QByteArray DiGiCoProtocol::stringElement(const QString& text) {
    QByteArray utf8 = text.toUtf8();
    QByteArray el;
    el.append('\x31'); // string type
    appendBE32(el, static_cast<quint32>(utf8.size() + 1)); // includes null terminator
    el += utf8;
    el.append('\0');
    return el;
}

QByteArray DiGiCoProtocol::int32Element(qint32 value) {
    QByteArray el;
    el.append('\x14'); // int32 type
    appendBE32(el, 4);
    appendBE32(el, static_cast<quint32>(value));
    return el;
}

QByteArray DiGiCoProtocol::byteElement(quint8 value) {
    QByteArray el;
    el.append('\x01'); // byte/bool type
    appendBE32(el, 1);
    el.append(static_cast<char>(value));
    return el;
}

QByteArray DiGiCoProtocol::buildFaderMessage(const QString& objectPath, double db) {
    qint32 centidB = static_cast<qint32>(std::lround(db * 100.0));
    centidB = qBound(kFaderOff, centidB, kFaderMax);
    return buildFrame("MMIX", kOpcodeSet, stringElement(objectPath) + int32Element(centidB));
}

QByteArray DiGiCoProtocol::buildMuteMessage(const QString& objectPath, bool muted) {
    return buildFrame("MMIX", kOpcodeSet,
                      stringElement(objectPath) + byteElement(muted ? 1 : 0));
}

QByteArray DiGiCoProtocol::buildSubscribeMixing() {
    QByteArray elements = stringElement("Mixing");
    elements.append('\x11'); // flag element type
    appendBE32(elements, 1);
    elements.append(static_cast<char>(0x80));
    return buildFrame("MMIX", kOpcodeSubscribe, elements);
}

QByteArray DiGiCoProtocol::buildSetClientType() {
    return buildFrame("EEVT", kOpcodeEvent,
                      stringElement("SetClientType") +
                          stringElement("TYPE:StageMix SYNCDIR:2"));
}

// --- connection ------------------------------------------------------------

bool DiGiCoProtocol::connect(const QString& host, int port) {
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

void DiGiCoProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_transport.disconnect();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void DiGiCoProtocol::sendHandshake() {
    // Register as a control client, then subscribe to the mixing tree so the
    // console streams fader/mute state back for tracking.
    m_transport.send(buildSetClientType());
    m_transport.send(buildSubscribeMixing());
}

// --- parameters ------------------------------------------------------------

void DiGiCoProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        const QString& kind = parts[0];
        bool ok = false;
        int index = parts[1].toInt(&ok);
        const QString& param = parts[2];

        if (ok && index >= 1) {
            QString object;
            if (kind == "ch")
                object = QString("Mixing/InputChannel%1").arg(objectIndex(index));
            else if (kind == "dca")
                object = QString("Mixing/DCA%1").arg(objectIndex(index));
            else if (kind == "bus" || kind == "mix")
                object = QString("Mixing/Mix%1").arg(objectIndex(index));

            if (!object.isEmpty()) {
                if (param == "fader") {
                    m_transport.send(buildFaderMessage(object + "/Fader/Level", value.toDouble()));
                } else if (param == "mute") {
                    m_transport.send(buildMuteMessage(object + "/Mute", value.toBool()));
                }
            }
        }
    }

    m_parameterCache[path] = value;
}

QVariant DiGiCoProtocol::getParameter(const QString& path) {
    return m_parameterCache.value(path);
}

void DiGiCoProtocol::requestParameter(const QString& path) {
    // state is pushed by the console after subscription, not polled
    Q_UNUSED(path);
}

void DiGiCoProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (!callback)
        return;
    if (m_parameterCache.contains(path))
        callback(path, m_parameterCache[path], true);
    else
        callback(path, QVariant(), false);
}

void DiGiCoProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void DiGiCoProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;
    // Snapshots recall by name over MSUP; scene-number recall is not exposed by
    // this protocol, so this is a no-op placeholder.
    Q_UNUSED(sceneNumber);
}

void DiGiCoProtocol::refresh() {
    if (m_connectionState == ConnectionState::Connected)
        m_transport.send(buildSubscribeMixing());
}

// --- receive ---------------------------------------------------------------

void DiGiCoProtocol::parseProtocolData(const QByteArray& data) {
    m_receiveBuffer.append(data);

    // Walk complete frames: tag(4) + length(4, BE) + body(length).
    while (m_receiveBuffer.size() >= 8) {
        quint32 length = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar*>(m_receiveBuffer.constData() + 4));

        if (static_cast<quint32>(m_receiveBuffer.size()) < 8 + length)
            break;

        QByteArray frame = m_receiveBuffer.left(8 + length);
        QByteArray tag = m_receiveBuffer.left(4);
        QByteArray body = m_receiveBuffer.mid(8, length);
        m_receiveBuffer.remove(0, 8 + length);

        // Keep the link alive by echoing the console's heartbeat frame.
        if (tag == "EEVT" && body.contains("KeepAlive"))
            m_transport.send(frame);
    }
}

// --- transport slots -------------------------------------------------------

void DiGiCoProtocol::onTransportConnected() {
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));
    sendHandshake();
    m_keepAliveTimer.start(KEEPALIVE_INTERVAL);
    emit connected();
}

void DiGiCoProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void DiGiCoProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);
}

void DiGiCoProtocol::onTransportConnectionLost() {
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void DiGiCoProtocol::onDataReceived(const QByteArray& data) { parseProtocolData(data); }

void DiGiCoProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected)
        m_transport.send(buildSubscribeMixing());
}

void DiGiCoProtocol::onReconnecting(int attempt, int maxAttempts) {
    setStatus(QString("Reconnecting (attempt %1/%2)...").arg(attempt).arg(maxAttempts));
}

void DiGiCoProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void DiGiCoProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
