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

namespace {
constexpr char ACE_F0 = '\xf0';
constexpr char ACE_F7 = '\xf7';
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
    // control-relevant subset of the ACE handshake (identity gate + the three
    // objects whose handles the control path addresses). The full reference
    // handshake also subscribes name/colour/metering objects used only for RX.
    return {QByteArrayLiteral("DR Box Identification"), QByteArrayLiteral("Surface Identification"),
            kInputMixer, kDcaLevelsAndMutes, kSceneManager};
}

double AllenHeathTcpProtocol::dbFromLevel(double level) {
    // OpenMix stores fader positions normalized 0..1; the console wants dB. Map
    // with 0 dB at 0.75 travel and +10 dB at the top, -inf at the bottom.
    level = std::clamp(level, 0.0, 1.0);
    if (level <= 0.0) {
        return -96.0; // below the encoder's -95 floor -> 0x0000
    }
    if (level >= 1.0) {
        return 10.0;
    }
    if (level < 0.75) {
        return -60.0 + (level / 0.75) * 60.0; // -60 .. 0 dB
    }
    return (level - 0.75) / 0.25 * 10.0; // 0 .. +10 dB
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

QByteArray AllenHeathTcpProtocol::buildChannelLevel(const QByteArray& handle, int channel,
                                                    double dB) const {
    if (handle.isEmpty() || channel < 1) {
        return {};
    }
    // F0 00 00 <handle:2> 00 00 <op> <ch-1> 00 02 <dB:2> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append('\0');
    f.append('\0');
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
    // F0 00 00 <handle:2> 00 00 <op> <(ch-1)+plane> 00 01 <on> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append('\0');
    f.append('\0');
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
    // F0 00 00 <handle:2> 00 00 10 <(dca-1)+plane> 00 01 <on> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append('\0');
    f.append('\0');
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
    // F0 00 00 <bankHandle:2> 00 00 12 <((ch-1)&7)+0x80> 00 02 <dB:2> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(bankHandle);
    f.append('\0');
    f.append('\0');
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
    // F0 00 00 <bankHandle:2> 00 00 12 <((ch-1)&7)+0x98> 00 01 <on> F7
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(bankHandle);
    f.append('\0');
    f.append('\0');
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
    // F0 00 00 <handle:2> 00 00 10 00 00 02 <(scene-1):2 BE> F7
    const int s = sceneNumber - 1;
    QByteArray f;
    f.append(ACE_F0);
    f.append('\0');
    f.append('\0');
    f.append(handle);
    f.append('\0');
    f.append('\0');
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

    return m_transport.connect(host, port);
}

void AllenHeathTcpProtocol::disconnect() {
    m_keepAliveTimer.stop();

    // ACE teardown: E0 00 03 "BYE" E7
    if (m_transport.isConnected()) {
        m_transport.send(QByteArray::fromHex("e00003425945e7"));
    }
    m_transport.disconnect();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_handles.clear();
    m_subscribeQueue.clear();
    m_subscribeIndex = 0;
    m_handshakeComplete = false;
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathTcpProtocol::startHandshake() {
    // session seed, then subscribe to the objects whose handles we need to
    // address. The console replies to each with a 14-byte frame carrying the
    // object's 2-byte handle at offset 11.
    m_transport.send(QByteArray::fromHex("e000040203"));

    m_handles.clear();
    m_subscribeQueue = subscribeObjects();
    m_subscribeIndex = 0;
    m_handshakeComplete = false;
    sendNextSubscribe();
}

void AllenHeathTcpProtocol::sendNextSubscribe() {
    if (m_subscribeIndex >= m_subscribeQueue.size()) {
        // all handles learned: ready to control
        m_handshakeComplete = true;
        setConnectionState(ConnectionState::Connected);
        setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));
        m_keepAliveTimer.start(KEEPALIVE_INTERVAL);
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
                m_transport.send(buildChannelLevel(handleFor(kInputMixer), index,
                                                   dbFromLevel(value.toDouble())));
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

void AllenHeathTcpProtocol::setChannelFader(int channel, double level) {
    sendParameter(QString("/ch/%1/fader").arg(channel), level);
}

void AllenHeathTcpProtocol::setChannelMute(int channel, bool muted) {
    sendParameter(QString("/ch/%1/mute").arg(channel), muted);
}

void AllenHeathTcpProtocol::refresh() {}

// --- receive ---

void AllenHeathTcpProtocol::onDataReceived(const QByteArray& data) {
    m_receiveBuffer.append(data);

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
            m_receiveBuffer.remove(0, end + 1); // console control frame; no action
        } else {
            m_receiveBuffer.remove(0, 1); // padding / resync
        }
    }
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

    // other frames are parameter/metering feedback (not needed for the outbound
    // control path)
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
    if (m_connectionState == ConnectionState::Connected) {
        m_transport.send(QByteArray::fromHex("e000040203"));
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
