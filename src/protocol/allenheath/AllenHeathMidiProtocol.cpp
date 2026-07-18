#include "AllenHeathMidiProtocol.h"
#include "../../core/Cue.h"

namespace OpenMix {

AllenHeathMidiProtocol::AllenHeathMidiProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort; // 51325

    QObject::connect(&m_transport, &TcpTransport::connected, this,
                     &AllenHeathMidiProtocol::onTransportConnected);
    QObject::connect(&m_transport, &TcpTransport::disconnected, this,
                     &AllenHeathMidiProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &TcpTransport::connectionError, this,
                     &AllenHeathMidiProtocol::onTransportError);
    QObject::connect(&m_transport, &TcpTransport::connectionLost, this,
                     &AllenHeathMidiProtocol::onTransportConnectionLost);
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     &AllenHeathMidiProtocol::onDataReceived);
    QObject::connect(&m_transport, &TcpTransport::reconnecting, this,
                     &AllenHeathMidiProtocol::onReconnecting);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &AllenHeathMidiProtocol::onKeepAliveTimeout);
}

AllenHeathMidiProtocol::~AllenHeathMidiProtocol() { disconnect(); }

bool AllenHeathMidiProtocol::connect(const QString& host, int port) {
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

void AllenHeathMidiProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_transport.disconnect();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathMidiProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    if (path.startsWith("/dca/")) {
        QStringList parts = path.split('/');
        if (parts.size() >= 4) {
            int dca = parts[2].toInt();
            QString param = parts[3];

            if (param == "fader") {
                // DCA level: 14-bit NRPN (SQ Issue 5, Master Sends/Control p24)
                const quint16 midiValue = encodeLevel14(value.toDouble());
                QByteArray msg = buildNRPNMessage(0, DCA_LEVEL_MSB, DCA_LEVEL_LSB_BASE + dca - 1,
                                                  (midiValue >> 7) & 0x7F, midiValue & 0x7F);
                m_transport.send(msg);
            } else if (param == "mute") {
                // SQ mute is an NRPN; value-fine 1 = muted, 0 = unmuted (p11/p21)
                QByteArray msg =
                    buildNRPNMessage(0, DCA_MUTE_MSB, dca - 1, 0x00, value.toBool() ? 0x01 : 0x00);
                m_transport.send(msg);
            }
        }
    } else if (path.startsWith("/ch/")) {
        QStringList parts = path.split('/');
        if (parts.size() >= 4) {
            int ch = parts[2].toInt();
            QString param = parts[3];

            if (param == "fader") {
                // input-channel level to LR: 14-bit NRPN (SQ Issue 5 p22)
                const quint16 midiValue = encodeLevel14(value.toDouble());
                QByteArray msg = buildNRPNMessage(0, CH_LEVEL_TO_LR_MSB, ch - 1,
                                                  (midiValue >> 7) & 0x7F, midiValue & 0x7F);
                m_transport.send(msg);
            } else if (param == "mute") {
                QByteArray msg =
                    buildNRPNMessage(0, CH_MUTE_MSB, ch - 1, 0x00, value.toBool() ? 0x01 : 0x00);
                m_transport.send(msg);
            }
        }
    }

    m_parameterCache[path] = value;
}

void AllenHeathMidiProtocol::setChannelFaderDb(int channel, double dB) {
    // route through the path-based encoder above (keeps one wire-format code path)
    sendParameter(QString("/ch/%1/fader").arg(channel), dB);
}

void AllenHeathMidiProtocol::setChannelMute(int channel, bool muted) {
    sendParameter(QString("/ch/%1/mute").arg(channel), muted);
}

QVariant AllenHeathMidiProtocol::getParameter(const QString& path) {
    return m_parameterCache.value(path);
}

void AllenHeathMidiProtocol::requestParameter(const QString& path) {
    // MIDI doesn't have a standard query mechanism
    // params are typically pushed by the console
    Q_UNUSED(path);
}

void AllenHeathMidiProtocol::requestParameterAsync(const QString& path,
                                                   ParameterCallback callback) {
    // return cached value if available
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

void AllenHeathMidiProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void AllenHeathMidiProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QByteArray msg = buildSceneRecall(sceneNumber);
    if (!msg.isEmpty())
        m_transport.send(msg);
}

void AllenHeathMidiProtocol::refresh() {
    // MIDI doesn't support general refresh
    // console pushes updates automatically
}

namespace {

// Approximate Audio Taper Level Values (SQ MIDI Protocol Issue 5, p20), as
// <dB, VC, VF>. The curve is not expressible as a formula, unlike the linear
// taper, so the printed anchors are interpolated between.
struct TaperPoint {
    double dB;
    quint8 vc;
    quint8 vf;
};

constexpr TaperPoint kAudioTaper[] = {
    {-89, 0x01, 0x40}, {-85, 0x02, 0x00}, {-80, 0x02, 0x40}, {-75, 0x03, 0x40}, {-70, 0x04, 0x00},
    {-65, 0x05, 0x00}, {-60, 0x06, 0x00}, {-55, 0x07, 0x00}, {-50, 0x08, 0x00}, {-45, 0x0C, 0x00},
    {-40, 0x0F, 0x40}, {-38, 0x12, 0x40}, {-36, 0x15, 0x40}, {-35, 0x17, 0x00}, {-34, 0x19, 0x00},
    {-33, 0x1A, 0x40}, {-32, 0x1C, 0x00}, {-31, 0x1D, 0x40}, {-30, 0x1F, 0x00}, {-29, 0x20, 0x40},
    {-28, 0x22, 0x00}, {-27, 0x23, 0x40}, {-26, 0x25, 0x00}, {-25, 0x26, 0x40}, {-24, 0x28, 0x40},
    {-23, 0x2A, 0x00}, {-22, 0x2B, 0x40}, {-21, 0x2D, 0x00}, {-20, 0x2E, 0x40}, {-19, 0x30, 0x00},
    {-18, 0x31, 0x40}, {-17, 0x33, 0x00}, {-16, 0x34, 0x40}, {-15, 0x36, 0x00}, {-14, 0x38, 0x00},
    {-13, 0x39, 0x40}, {-12, 0x3B, 0x00}, {-11, 0x3C, 0x40}, {-10, 0x3E, 0x00}, {-9, 0x41, 0x40},
    {-8, 0x44, 0x40},  {-7, 0x48, 0x00},  {-6, 0x4B, 0x00},  {-5, 0x4E, 0x40},  {-4, 0x52, 0x40},
    {-3, 0x56, 0x40},  {-2, 0x5A, 0x00},  {-1, 0x5E, 0x00},  {0, 0x62, 0x00},   {1, 0x65, 0x40},
    {2, 0x69, 0x00},   {3, 0x6C, 0x40},   {4, 0x70, 0x00},   {5, 0x73, 0x40},   {6, 0x75, 0x40},
    {7, 0x78, 0x00},   {8, 0x7A, 0x40},   {9, 0x7D, 0x00},   {10, 0x7F, 0x40},
};

quint16 taperValue(const TaperPoint& p) { return static_cast<quint16>((p.vc << 7) | p.vf); }

} // namespace

quint16 AllenHeathMidiProtocol::encodeLinearTaper(double dB) {
    // the printed table is exactly linear in dB: 0 dB = 15196, +10 dB = 16383,
    // so 118.7 steps per dB. Anchors reproduce to within the doc's own rounding.
    if (dB <= NEG_INF_DB) {
        return 0;
    }
    const int v = static_cast<int>(std::lround(15196.0 + 118.7 * dB));
    return static_cast<quint16>(std::clamp(v, 0, 16383));
}

quint16 AllenHeathMidiProtocol::encodeAudioTaper(double dB) {
    if (dB <= NEG_INF_DB) {
        return 0;
    }
    const auto* first = std::begin(kAudioTaper);
    const auto* last = std::end(kAudioTaper) - 1;
    if (dB <= first->dB) {
        return taperValue(*first);
    }
    if (dB >= last->dB) {
        return taperValue(*last);
    }
    for (const TaperPoint* p = first; p < last; ++p) {
        const TaperPoint* next = p + 1;
        if (dB >= p->dB && dB <= next->dB) {
            const double span = next->dB - p->dB;
            const double t = span > 0.0 ? (dB - p->dB) / span : 0.0;
            const double lo = taperValue(*p);
            const double hi = taperValue(*next);
            return static_cast<quint16>(std::lround(lo + t * (hi - lo)));
        }
    }
    return taperValue(*last);
}

quint16 AllenHeathMidiProtocol::encodeLevel14(double dB) const {
    return m_faderLaw == FaderLaw::AudioTaper ? encodeAudioTaper(dB) : encodeLinearTaper(dB);
}

QByteArray AllenHeathMidiProtocol::buildNRPNMessage(int channel, int nrpnMsb, int nrpnLsb,
                                                    int valueMsb, int valueLsb) {
    QByteArray msg;
    channel = channel & 0x0F;

    // NRPN MSB (CC 99)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(99));
    msg.append(static_cast<char>(nrpnMsb & 0x7F));

    // NRPN LSB (CC 98)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(98));
    msg.append(static_cast<char>(nrpnLsb & 0x7F));

    // data entry MSB (CC 6)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(6));
    msg.append(static_cast<char>(valueMsb & 0x7F));

    // data entry LSB (CC 38)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(38));
    msg.append(static_cast<char>(valueLsb & 0x7F));

    return msg;
}

// Allen & Heath scene recall over MIDI is a Bank Select (CC0) followed by a
// Program Change, per the A&H MIDI Protocol. sceneNumber is the 1-based scene as
// shown on the console; MIDI values are offset by -1, and scenes split into
// banks of 128 (1-128 -> bank 0, 129-256 -> bank 1, 257-300 -> bank 2).
QByteArray AllenHeathMidiProtocol::buildSceneRecall(int sceneNumber) {
    const int index = sceneNumber - 1;
    if (index < 0)
        return {}; // "None" / unset

    const int bank = index / 128;
    const int program = index % 128;
    const int channel = 0; // MIDI channel 1

    QByteArray msg;
    msg.append(static_cast<char>(0xB0 | channel)); // Control Change
    msg.append(static_cast<char>(0x00));           // CC 0 = Bank Select MSB
    msg.append(static_cast<char>(bank & 0x7F));
    msg.append(static_cast<char>(0xC0 | channel)); // Program Change
    msg.append(static_cast<char>(program & 0x7F));

    return msg;
}

QByteArray AllenHeathMidiProtocol::buildControlChange(int channel, int cc, int value) {
    QByteArray msg;
    msg.append(static_cast<char>(0xB0 | (channel & 0x0F)));
    msg.append(static_cast<char>(cc & 0x7F));
    msg.append(static_cast<char>(value & 0x7F));
    return msg;
}

void AllenHeathMidiProtocol::parseMidiData(const QByteArray& data) {
    m_receiveBuffer.append(data);

    while (!m_receiveBuffer.isEmpty()) {
        unsigned char status = static_cast<unsigned char>(m_receiveBuffer[0]);

        if (status == 0xF0) {
            int endPos = m_receiveBuffer.indexOf(static_cast<char>(0xF7));
            if (endPos < 0)
                break; // incomplete

            QByteArray sysex = m_receiveBuffer.left(endPos + 1);
            m_receiveBuffer.remove(0, endPos + 1);

            processSysEx(sysex);
        } else if ((status & 0xF0) == 0xB0) {
            if (m_receiveBuffer.size() < 3)
                break;

            int channel = status & 0x0F;
            int cc = static_cast<unsigned char>(m_receiveBuffer[1]);
            int value = static_cast<unsigned char>(m_receiveBuffer[2]);
            m_receiveBuffer.remove(0, 3);

            processControlChange(channel, cc, value);
        } else if ((status & 0x80) == 0) {
            m_receiveBuffer.remove(0, 1);
        } else {
            int len = 3;
            if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
                len = 2;
            }
            if (m_receiveBuffer.size() < len)
                break;
            m_receiveBuffer.remove(0, len);
        }
    }
}

void AllenHeathMidiProtocol::processControlChange(int channel, int cc, int value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_nrpnState.timestamp > 0 && (now - m_nrpnState.timestamp) > NRPN_TIMEOUT_MS) {
        m_nrpnState.reset();
    }

    // NRPN message sequence: CC 99 (MSB), CC 98 (LSB), CC 6 (data MSB), CC 38 (data LSB)
    switch (cc) {
    case 99:
        m_nrpnState.reset();
        m_nrpnState.channel = channel;
        m_nrpnState.nrpnMsb = value;
        m_nrpnState.timestamp = now;
        break;

    case 98:
        if (m_nrpnState.channel == channel) {
            m_nrpnState.nrpnLsb = value;
            m_nrpnState.timestamp = now;
        }
        break;

    case 6:
        if (m_nrpnState.channel == channel) {
            m_nrpnState.dataMsb = value;
            m_nrpnState.timestamp = now;
            if (m_nrpnState.isComplete()) {
                processNRPNComplete();
            }
        }
        break;

    case 38:
        if (m_nrpnState.channel == channel && m_nrpnState.dataMsb >= 0) {
            m_nrpnState.dataLsb = value;
            m_nrpnState.timestamp = now;
            if (m_nrpnState.isComplete()) {
                processNRPNComplete();
            }
        }
        break;

    default:
        // Allen & Heath DCA mutes: CC 0x50-0x57 (80-87) for DCAs 1-8
        if (cc >= 0x50 && cc <= 0x57) {
            int dca = cc - 0x50 + 1;
            bool muted = value >= 64;
            QString path = QString("/dca/%1/mute").arg(dca);
            m_parameterCache[path] = muted;
            emit parameterChanged(path, muted);
        }
        break;
    }
}

void AllenHeathMidiProtocol::processNRPNComplete() {
    int dataValue =
        (m_nrpnState.dataMsb << 7) | (m_nrpnState.dataLsb >= 0 ? m_nrpnState.dataLsb : 0);

    // Allen & Heath NRPN mapping for DCA faders
    // NRPN MSB 0x63 (99), LSB = DCA index (0-7 for DCAs 1-8)
    if (m_nrpnState.nrpnMsb == 0x63 && m_nrpnState.nrpnLsb >= 0 && m_nrpnState.nrpnLsb <= 7) {
        int dca = m_nrpnState.nrpnLsb + 1;
        float level = dataValue / 16383.0f;
        QString path = QString("/dca/%1/fader").arg(dca);
        m_parameterCache[path] = level;
        emit parameterChanged(path, level);
    }

    m_nrpnState.reset();
}

void AllenHeathMidiProtocol::processSysEx(const QByteArray& sysex) {
    // Allen & Heath SysEx format: F0 00 00 1A [device] [cmd] [data...] F7
    if (sysex.size() < 7)
        return;

    unsigned char byte3 = static_cast<unsigned char>(sysex[3]);
    if (byte3 != 0x1A)
        return; // not Allen & Heath

    unsigned char device = static_cast<unsigned char>(sysex[4]);
    unsigned char cmd = static_cast<unsigned char>(sysex[5]);

    Q_UNUSED(device);

    if (cmd == 0x10 && sysex.size() >= 9) {
        int sceneNumber = static_cast<unsigned char>(sysex[8]);
        emit sceneChanged(sceneNumber);
    }
}

void AllenHeathMidiProtocol::onTransportConnected() {
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

    m_keepAliveTimer.start(KEEPALIVE_INTERVAL);

    emit connected();
}

void AllenHeathMidiProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathMidiProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);
}

void AllenHeathMidiProtocol::onTransportConnectionLost() {
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void AllenHeathMidiProtocol::onDataReceived(const QByteArray& data) { parseMidiData(data); }

void AllenHeathMidiProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        // send active sensing (0xFE) as keep-alive
        QByteArray keepAlive;
        keepAlive.append(static_cast<char>(0xFE));
        m_transport.send(keepAlive);
    }
}

void AllenHeathMidiProtocol::onReconnecting(int attempt, int maxAttempts) {
    setStatus(QString("Reconnecting (attempt %1/%2)...").arg(attempt).arg(maxAttempts));
}

void AllenHeathMidiProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void AllenHeathMidiProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
