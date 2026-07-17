#include "AllenHeathTcpProtocol.h"
#include "../../core/Cue.h"
#include <QtEndian>
#include <cmath>

namespace OpenMix {

// object names whose handles the control path needs (learned during handshake)
const QByteArray AllenHeathTcpProtocol::kInputMixer = QByteArrayLiteral("Input Mixer");
const QByteArray AllenHeathTcpProtocol::kDcaLevelsAndMutes =
    QByteArrayLiteral("DCA Levels and Mutes");
const QByteArray AllenHeathTcpProtocol::kSceneManager = QByteArrayLiteral("StageBox Scene Manager");
const QByteArray AllenHeathTcpProtocol::kMeteringSources = QByteArrayLiteral("Metering Sources");

namespace {
constexpr char ACE_F0 = '\xf0';
constexpr char ACE_F7 = '\xf7';

// the console's half of the port exchange; the client's seed carries 01 here
const QByteArray kSessionReply = QByteArrayLiteral("\xe0\x00\x04\x02\x03");
} // namespace

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
    QObject::connect(&m_rxWatchdogTimer, &QTimer::timeout, this,
                     &AllenHeathTcpProtocol::onRxWatchdogTimeout);

    m_handshakeTimer.setSingleShot(true);
    QObject::connect(&m_handshakeTimer, &QTimer::timeout, this,
                     &AllenHeathTcpProtocol::onHandshakeTimeout);

    QObject::connect(&m_udpSocket, &QUdpSocket::readyRead, this,
                     &AllenHeathTcpProtocol::onUdpDataReceived);
}

AllenHeathTcpProtocol::~AllenHeathTcpProtocol() { disconnect(); }

// --- value + frame builders (byte layouts recovered from AvantisDriver/DLiveDriver) ---

QByteArray AllenHeathTcpProtocol::encodeDbTables(double dB, const quint8 posLo[10],
                                                 const quint8 negLo[10]) {
    // Exact reproduction of the reference encoders: the high byte is
    // (intDB + 0x80); the low byte is a per-0.1 dB lookup (distinct tables for
    // positive vs negative tenths). Negative values with a nonzero tenth floor
    // the integer part. Usable range -95..+10 dB; <= -95 -> -inf (0x0000).
    if (dB <= -95.0) {
        return QByteArray::fromHex("0000"); // -inf
    }
    if (dB >= 10.0) {
        QByteArray b;
        b.append(static_cast<char>(0x8A)); // +10 dB
        b.append(static_cast<char>(0x00));
        return b;
    }

    const int tenths = static_cast<int>(std::lround(dB * 10.0));
    const int fracDigit = std::abs(tenths % 10);

    int intPart = tenths / 10; // truncates toward zero
    quint8 lo;
    if (tenths >= 0) {
        lo = posLo[fracDigit];
    } else {
        if (fracDigit != 0) {
            intPart -= 1; // floor for negative non-integer dB
        }
        lo = negLo[fracDigit];
    }

    QByteArray b;
    b.append(static_cast<char>((intPart + 0x80) & 0xFF));
    b.append(static_cast<char>(lo));
    return b;
}

QByteArray AllenHeathTcpProtocol::encodeDb(double dB) const {
    // Avantis/dLive tables (reference FUN_00415330)
    static const quint8 kPosLo[10] = {0x00, 0x1C, 0x36, 0x4F, 0x69, 0x82, 0x9C, 0xB6, 0xCF, 0xE9};
    static const quint8 kNegLo[10] = {0x00, 0xD9, 0xBF, 0xA6, 0x8C, 0x73, 0x59, 0x40, 0x26, 0x0C};
    return encodeDbTables(dB, kPosLo, kNegLo);
}

QList<QByteArray> AllenHeathTcpProtocol::subscribeObjects() const {
    // subset of the reference's chain: the identity gate, the three objects the
    // control path addresses, and the metering subscribe that starts the UDP meter
    // stream. The reference also subscribes name/colour objects we do not read.
    return {QByteArrayLiteral("DR Box Identification"),
            QByteArrayLiteral("Surface Identification"),
            kInputMixer,
            kDcaLevelsAndMutes,
            kSceneManager,
            kMeteringSources};
}

QByteArray AllenHeathTcpProtocol::buildSubscribe(const QByteArray& objectName) {
    // F0 00 01 00 00 00 01 00 04 00 <len=name+NUL> <name> 00 F7
    QByteArray f = QByteArray::fromHex("f0000100000001000400");
    f.append(static_cast<char>(objectName.size() + 1));
    f.append(objectName);
    f.append('\0');
    f.append(ACE_F7);
    return f;
}

QByteArray AllenHeathTcpProtocol::buildSessionSeed(quint16 udpPort) {
    QByteArray f = QByteArray::fromHex("e000040103");
    f.append(static_cast<char>((udpPort >> 8) & 0xFF));
    f.append(static_cast<char>(udpPort & 0xFF));
    f.append('\xe7');
    return f;
}

QByteArray AllenHeathTcpProtocol::buildKeepAlive() { return QByteArray::fromHex("e0000103e7"); }

quint16 AllenHeathTcpProtocol::parseSessionReply(const QByteArray& frame) {
    if (frame.size() < 8 || !frame.startsWith(kSessionReply)) {
        return 0;
    }
    return static_cast<quint16>((static_cast<quint8>(frame.at(5)) << 8) |
                                static_cast<quint8>(frame.at(6)));
}

QByteArray AllenHeathTcpProtocol::buildChannelLevel(const QByteArray& handle, int channel,
                                                    double dB) const {
    if (handle.isEmpty() || channel < 1) {
        return {};
    }
    // F0 00 00 <handle:2> <src:2> <op> <ch-1> 00 02 <dB:2> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append(static_cast<char>((SOURCE_ID >> 8) & 0xFF));
    f.append(static_cast<char>(SOURCE_ID & 0xFF));
    f.append(static_cast<char>(channelLevelOp()));
    f.append(static_cast<char>((channel - 1) & 0xFF));
    f.append('\0');
    f.append('\x02');
    f.append(encodeDb(dB));
    f.append(ACE_F7);
    return f;
}

QByteArray AllenHeathTcpProtocol::buildChannelMute(const QByteArray& handle, int channel,
                                                   bool on) const {
    if (handle.isEmpty() || channel < 1) {
        return {};
    }
    // F0 00 00 <handle:2> <src:2> <op> <(ch-1)+plane> 00 01 <on> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append(static_cast<char>((SOURCE_ID >> 8) & 0xFF));
    f.append(static_cast<char>(SOURCE_ID & 0xFF));
    f.append(static_cast<char>(channelMuteOp()));
    f.append(static_cast<char>(((channel - 1) + channelMutePlane()) & 0xFF));
    f.append('\0');
    f.append('\x01');
    f.append(static_cast<char>(on ? 1 : 0));
    f.append(ACE_F7);
    return f;
}

QByteArray AllenHeathTcpProtocol::buildDcaMute(const QByteArray& handle, int dca, bool on) const {
    if (handle.isEmpty() || dca < 1) {
        return {};
    }
    // F0 00 00 <handle:2> <src:2> 10 <(dca-1)+plane> 00 01 <on> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append(static_cast<char>((SOURCE_ID >> 8) & 0xFF));
    f.append(static_cast<char>(SOURCE_ID & 0xFF));
    f.append('\x10');
    f.append(static_cast<char>(((dca - 1) + dcaMutePlane()) & 0xFF));
    f.append('\0');
    f.append('\x01');
    f.append(static_cast<char>(on ? 1 : 0));
    f.append(ACE_F7);
    return f;
}

QByteArray AllenHeathTcpProtocol::buildChannelLevelSpill(const QByteArray& bankHandle, int channel,
                                                         double dB) const {
    if (bankHandle.isEmpty() || channel < 1) {
        return {};
    }
    // F0 00 00 <bankHandle:2> <src:2> 12 <((ch-1)&7)+0x80> 00 02 <dB:2> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(bankHandle);
    f.append(static_cast<char>((SOURCE_ID >> 8) & 0xFF));
    f.append(static_cast<char>(SOURCE_ID & 0xFF));
    f.append('\x12');
    f.append(static_cast<char>((((channel - 1) & 0x07) + 0x80) & 0xFF));
    f.append('\0');
    f.append('\x02');
    f.append(encodeDb(dB));
    f.append(ACE_F7);
    return f;
}

QByteArray AllenHeathTcpProtocol::buildChannelMuteSpill(const QByteArray& bankHandle, int channel,
                                                        bool on) const {
    if (bankHandle.isEmpty() || channel < 1) {
        return {};
    }
    // F0 00 00 <bankHandle:2> <src:2> 12 <((ch-1)&7)+0x98> 00 01 <on> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(bankHandle);
    f.append(static_cast<char>((SOURCE_ID >> 8) & 0xFF));
    f.append(static_cast<char>(SOURCE_ID & 0xFF));
    f.append('\x12');
    f.append(static_cast<char>((((channel - 1) & 0x07) + 0x98) & 0xFF));
    f.append('\0');
    f.append('\x01');
    f.append(static_cast<char>(on ? 1 : 0));
    f.append(ACE_F7);
    return f;
}

QByteArray AllenHeathTcpProtocol::buildSceneRecall(const QByteArray& handle, int sceneNumber) {
    if (handle.isEmpty() || sceneNumber < 1) {
        return {};
    }
    // F0 00 00 <handle:2> <src:2> 10 00 00 02 <(scene-1):2 BE> F7
    const int s = sceneNumber - 1;
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append(static_cast<char>((SOURCE_ID >> 8) & 0xFF));
    f.append(static_cast<char>(SOURCE_ID & 0xFF));
    f.append('\x10');
    f.append('\0');
    f.append('\0');
    f.append('\x02');
    f.append(static_cast<char>((s >> 8) & 0xFF));
    f.append(static_cast<char>(s & 0xFF));
    f.append(ACE_F7);
    return f;
}

// --- connection ---

bool AllenHeathTcpProtocol::connect(const QString& host, int port) {
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(port));

    // the seed carries our UDP local port, so bind before connecting
    if (m_udpSocket.state() != QAbstractSocket::BoundState &&
        !m_udpSocket.bind(QHostAddress::Any, 0,
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        const QString error =
            QString("Failed to bind metering socket: %1").arg(m_udpSocket.errorString());
        setStatus(error);
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError(error);
        return false;
    }

    return m_transport.connect(host, port);
}

void AllenHeathTcpProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_rxWatchdogTimer.stop();
    m_handshakeTimer.stop();

    // ACE teardown: E0 00 03 "BYE" E7
    if (m_transport.isConnected()) {
        m_transport.send(QByteArray::fromHex("e00003425945e7"));
    }
    m_transport.disconnect();
    m_udpSocket.close();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_handles.clear();
    m_subscribeQueue.clear();
    m_subscribeIndex = 0;
    m_handshakeComplete = false;
    m_sessionOpen = false;
    m_consoleUdpPort = 0;
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathTcpProtocol::startHandshake() {
    // the console accepts nothing until the port exchange completes; the subscribe
    // chain starts in handleControlFrame()
    m_handles.clear();
    m_subscribeQueue = subscribeObjects();
    m_subscribeIndex = 0;
    m_handshakeComplete = false;
    m_sessionOpen = false;
    m_consoleUdpPort = 0;

    m_lastRxTimer.start();
    m_handshakeTimer.start(HANDSHAKE_TIMEOUT);

    m_transport.send(buildSessionSeed(m_udpSocket.localPort()));
}

void AllenHeathTcpProtocol::failHandshake(const QString& reason) {
    m_handshakeTimer.stop();
    m_keepAliveTimer.stop();
    m_rxWatchdogTimer.stop();
    m_transport.disconnect();
    m_udpSocket.close();
    m_handshakeComplete = false;
    m_sessionOpen = false;

    setConnectionState(ConnectionState::Disconnected);
    setStatus(reason);
    emit connectionError(reason);
}

void AllenHeathTcpProtocol::sendNextSubscribe() {
    if (m_subscribeIndex >= m_subscribeQueue.size()) {
        // all handles learned: ready to control
        m_handshakeComplete = true;
        m_handshakeTimer.stop();
        setConnectionState(ConnectionState::Connected);
        setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));
        emit connected();
        return;
    }
    m_transport.send(buildSubscribe(m_subscribeQueue.at(m_subscribeIndex)));
}

// --- parameter operations ---

void AllenHeathTcpProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    const QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        bool ok = false;
        const int index = parts[1].toInt(&ok);
        const QString param = parts[2];

        if (ok && parts[0] == "ch") {
            if (param == "fader") {
                m_transport.send(
                    buildChannelLevel(handleFor(kInputMixer), index, value.toDouble()));
            } else if (param == "mute") {
                m_transport.send(buildChannelMute(handleFor(kInputMixer), index, value.toBool()));
            }
        } else if (ok && parts[0] == "dca") {
            if (param == "mute") {
                m_transport.send(
                    buildDcaMute(handleFor(kDcaLevelsAndMutes), index, value.toBool()));
            }
            // DCA fader is not transmitted on ACE (receive-only plane)
        }
    }

    m_parameterCache[path] = value;
}

QVariant AllenHeathTcpProtocol::getParameter(const QString& path) {
    return m_parameterCache.value(path);
}

void AllenHeathTcpProtocol::requestParameter(const QString& path) { Q_UNUSED(path); }

void AllenHeathTcpProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (!callback) {
        return;
    }
    if (m_parameterCache.contains(path)) {
        callback(path, m_parameterCache[path], true);
    } else {
        callback(path, QVariant(), false);
    }
}

void AllenHeathTcpProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }
    const QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void AllenHeathTcpProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }
    m_transport.send(buildSceneRecall(handleFor(kSceneManager), sceneNumber));
}

void AllenHeathTcpProtocol::setChannelFaderDb(int channel, double level) {
    sendParameter(QString("/ch/%1/fader").arg(channel), level);
}

void AllenHeathTcpProtocol::setChannelMute(int channel, bool muted) {
    sendParameter(QString("/ch/%1/mute").arg(channel), muted);
}

void AllenHeathTcpProtocol::refresh() {}

// --- receive ---

void AllenHeathTcpProtocol::onDataReceived(const QByteArray& data) {
    m_receiveBuffer.append(data);
    m_lastRxTimer.restart();

    // property frames are length-prefixed: an F0 frame's total length is the
    // 16-bit big-endian payload length at bytes [9..10] plus 0x0C of header/tail.
    // E0 control frames are E7-terminated. Skip anything else (padding).
    while (!m_receiveBuffer.isEmpty()) {
        const quint8 lead = static_cast<quint8>(m_receiveBuffer.at(0));

        if (lead == 0xF0) {
            if (m_receiveBuffer.size() < 11) {
                break; // need the length field
            }
            const int payloadLen = (static_cast<quint8>(m_receiveBuffer.at(9)) << 8) |
                                   static_cast<quint8>(m_receiveBuffer.at(10));
            const int frameLen = payloadLen + 0x0C;
            if (m_receiveBuffer.size() < frameLen) {
                break; // wait for the rest
            }
            handleAceFrame(m_receiveBuffer.left(frameLen));
            m_receiveBuffer.remove(0, frameLen);
        } else if (lead == 0xE0) {
            const int end = m_receiveBuffer.indexOf('\xe7', 1);
            if (end < 0) {
                break;
            }
            handleControlFrame(m_receiveBuffer.left(end + 1));
            m_receiveBuffer.remove(0, end + 1);
        } else {
            m_receiveBuffer.remove(0, 1); // padding / resync
        }
    }
}

void AllenHeathTcpProtocol::handleControlFrame(const QByteArray& frame) {
    const quint16 port = parseSessionReply(frame);
    if (port == 0 || m_sessionOpen) {
        return;
    }

    m_consoleUdpPort = port;
    m_sessionOpen = true;
    m_keepAliveTimer.start(KEEPALIVE_INTERVAL);
    m_rxWatchdogTimer.start(RX_WATCHDOG_INTERVAL);
    sendNextSubscribe();
}

void AllenHeathTcpProtocol::onUdpDataReceived() {
    while (m_udpSocket.hasPendingDatagrams()) {
        QByteArray datagram(static_cast<int>(m_udpSocket.pendingDatagramSize()), '\0');
        QHostAddress sender;
        m_udpSocket.readDatagram(datagram.data(), datagram.size(), &sender);

        // the socket is dual-stack, so the console's IPv4 address can arrive
        // v4-mapped; compare tolerantly or every datagram is discarded as foreign
        if (sender.isEqual(QHostAddress(m_host), QHostAddress::TolerantConversion)) {
            handleMeterDatagram(datagram);
        }
    }
}

void AllenHeathTcpProtocol::handleMeterDatagram(const QByteArray& datagram) {
    // meters arrive as one 8-byte record per channel, in channel order: the record
    // index is the channel, there is no index field on the wire
    if (datagram.size() < METER_MIN_DATAGRAM || areaOf(datagram) != AREA_METERING) {
        return;
    }

    const QByteArray records = datagram.mid(ACE_HEADER_SIZE);
    const int count = qMin(METER_CHANNELS, records.size() / METER_RECORD_SIZE);
    for (int i = 0; i < count; ++i) {
        const quint8 raw = static_cast<quint8>(records.at(i * METER_RECORD_SIZE));
        const float level = (raw == 0 || raw >= METER_INVALID) ? 0.0f : raw / 253.0f;
        emit channelMeter(i + 1, level);
    }
}

quint16 AllenHeathTcpProtocol::areaOf(const QByteArray& frame) {
    if (frame.size() < 5) {
        return 0;
    }
    return static_cast<quint16>((static_cast<quint8>(frame.at(3)) << 8) |
                                static_cast<quint8>(frame.at(4)));
}

void AllenHeathTcpProtocol::handleAceFrame(const QByteArray& frame) {
    // A subscribe reply is exactly 14 bytes with the reply marker 00 01 at [1..2]
    // (console->app; app->console SET frames use 00 00) and carries the learned
    // object handle at [11..12]. Correlation is by the order we sent subscribes.
    const bool isHandleReply = frame.size() == 14 && static_cast<quint8>(frame.at(1)) == 0x00 &&
                               static_cast<quint8>(frame.at(2)) == 0x01;

    if (!m_handshakeComplete && m_subscribeIndex < m_subscribeQueue.size() && isHandleReply) {
        m_handles.insert(m_subscribeQueue.at(m_subscribeIndex), frame.mid(11, 2));
        ++m_subscribeIndex;
        sendNextSubscribe();
        return;
    }

    if (areaOf(frame) == AREA_SCENE_CHANGED) {
        const int scene = parseSceneChanged(frame);
        if (scene > 0) {
            emit sceneChanged(scene);
        }
    }
}

int AllenHeathTcpProtocol::parseSceneChanged(const QByteArray& frame) {
    // payload is the 0-based scene index, big-endian
    if (frame.size() < ACE_HEADER_SIZE + 2 || areaOf(frame) != AREA_SCENE_CHANGED) {
        return 0;
    }
    const int index = (static_cast<quint8>(frame.at(ACE_HEADER_SIZE)) << 8) |
                      static_cast<quint8>(frame.at(ACE_HEADER_SIZE + 1));
    return index + 1;
}

void AllenHeathTcpProtocol::onTransportConnected() { startHandshake(); }

void AllenHeathTcpProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    m_handshakeComplete = false;
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathTcpProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);
}

void AllenHeathTcpProtocol::onTransportConnectionLost() {
    m_handshakeComplete = false;
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void AllenHeathTcpProtocol::onKeepAliveTimeout() {
    if (m_sessionOpen && m_consoleUdpPort != 0) {
        m_udpSocket.writeDatagram(buildKeepAlive(), QHostAddress(m_host), m_consoleUdpPort);
    }
}

void AllenHeathTcpProtocol::onRxWatchdogTimeout() {
    if (m_sessionOpen && m_lastRxTimer.isValid() && m_lastRxTimer.elapsed() > RX_SILENCE_LIMIT) {
        failHandshake(
            QString("Console stopped responding (no data for %1ms)").arg(RX_SILENCE_LIMIT));
    }
}

void AllenHeathTcpProtocol::onHandshakeTimeout() {
    if (m_handshakeComplete) {
        return;
    }
    failHandshake(m_sessionOpen ? "Console did not complete the subscribe handshake"
                                : "Console did not answer the session handshake");
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
