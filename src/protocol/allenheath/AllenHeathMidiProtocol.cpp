#include "AllenHeathMidiProtocol.h"
#include "../../core/Cue.h"
#include <algorithm>
#include <cmath>

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

    // wire trace for the connection log
    QObject::connect(&m_transport, &TcpTransport::bytesSent, this,
                     [this](const QByteArray& b) { emit wireTrace(true, b); });
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     [this](const QByteArray& b) { emit wireTrace(false, b); });

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &AllenHeathMidiProtocol::onKeepAliveTimeout);

    m_nrpnFlushTimer.setSingleShot(true);
    m_nrpnFlushTimer.setInterval(NRPN_TIMEOUT_MS);
    QObject::connect(&m_nrpnFlushTimer, &QTimer::timeout, this,
                     &AllenHeathMidiProtocol::onNrpnFlush);
}

AllenHeathMidiProtocol::~AllenHeathMidiProtocol() { disconnect(); }

void AllenHeathMidiProtocol::setMidiChannel(int channel1To16) {
    m_midiChannel = std::clamp(channel1To16, 1, 16) - 1;
}

bool AllenHeathMidiProtocol::connect(const QString& host, int port) {
    if (m_connectionState != ConnectionState::Disconnected) {
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
    m_nrpnFlushTimer.stop();
    // leave the connected state before the socket closes so the teardown is
    // not mistaken for a lost link
    setConnectionState(ConnectionState::Disconnected);
    m_transport.disconnect();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_nrpnState.reset();
    m_latencyMs = 0;

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
                QByteArray msg = buildNRPNMessage(DCA_LEVEL_MSB, DCA_LEVEL_LSB_BASE + dca - 1,
                                                  (midiValue >> 7) & 0x7F, midiValue & 0x7F);
                m_transport.send(msg);
            } else if (param == "mute") {
                // SQ mute is an NRPN; value-fine 1 = muted, 0 = unmuted (p11/p21)
                QByteArray msg =
                    buildNRPNMessage(DCA_MUTE_MSB, dca - 1, 0x00, value.toBool() ? 0x01 : 0x00);
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
                QByteArray msg = buildNRPNMessage(CH_LEVEL_TO_LR_MSB, ch - 1,
                                                  (midiValue >> 7) & 0x7F, midiValue & 0x7F);
                m_transport.send(msg);
            } else if (param == "mute") {
                QByteArray msg =
                    buildNRPNMessage(CH_MUTE_MSB, ch - 1, 0x00, value.toBool() ? 0x01 : 0x00);
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

std::optional<double> AllenHeathMidiProtocol::readChannelFader(int channel) {
    const QVariant value = m_parameterCache.value(QString("/ch/%1/fader").arg(channel));
    if (!value.isValid())
        return std::nullopt;
    return value.toDouble();
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

// the printed linear table is exactly linear in dB: 0 dB = 15196, +10 dB = 16383
constexpr double kLinearZeroDb = 15196.0;
constexpr double kLinearStepsPerDb = 118.7;

} // namespace

quint16 AllenHeathMidiProtocol::encodeLinearTaper(double dB) {
    // Anchors reproduce to within the doc's own rounding.
    if (dB <= NEG_INF_DB) {
        return 0;
    }
    const int v = static_cast<int>(std::lround(kLinearZeroDb + kLinearStepsPerDb * dB));
    return static_cast<quint16>(std::clamp(v, 0, 16383));
}

double AllenHeathMidiProtocol::decodeLinearTaper(quint16 value) {
    if (value == 0) {
        return NEG_INF_DB;
    }
    return (value - kLinearZeroDb) / kLinearStepsPerDb;
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

double AllenHeathMidiProtocol::decodeAudioTaper(quint16 value) {
    if (value == 0) {
        return NEG_INF_DB;
    }
    const auto* first = std::begin(kAudioTaper);
    const auto* last = std::end(kAudioTaper) - 1;
    if (value <= taperValue(*first)) {
        return first->dB;
    }
    if (value >= taperValue(*last)) {
        return last->dB;
    }
    for (const TaperPoint* p = first; p < last; ++p) {
        const TaperPoint* next = p + 1;
        const double lo = taperValue(*p);
        const double hi = taperValue(*next);
        if (value >= lo && value <= hi) {
            const double t = hi > lo ? (value - lo) / (hi - lo) : 0.0;
            return p->dB + t * (next->dB - p->dB);
        }
    }
    return last->dB;
}

quint16 AllenHeathMidiProtocol::encodeLevel14(double dB) const {
    return m_faderLaw == FaderLaw::AudioTaper ? encodeAudioTaper(dB) : encodeLinearTaper(dB);
}

double AllenHeathMidiProtocol::decodeLevel14(quint16 value) const {
    return m_faderLaw == FaderLaw::AudioTaper ? decodeAudioTaper(value)
                                              : decodeLinearTaper(value);
}

QByteArray AllenHeathMidiProtocol::buildNRPNMessage(int nrpnMsb, int nrpnLsb, int valueMsb,
                                                    int valueLsb) const {
    QByteArray msg;
    const char status = static_cast<char>(0xB0 | (m_midiChannel & 0x0F));

    // NRPN MSB (CC 99)
    msg.append(status);
    msg.append(static_cast<char>(99));
    msg.append(static_cast<char>(nrpnMsb & 0x7F));

    // NRPN LSB (CC 98)
    msg.append(status);
    msg.append(static_cast<char>(98));
    msg.append(static_cast<char>(nrpnLsb & 0x7F));

    // data entry MSB (CC 6)
    msg.append(status);
    msg.append(static_cast<char>(6));
    msg.append(static_cast<char>(valueMsb & 0x7F));

    // data entry LSB (CC 38)
    msg.append(status);
    msg.append(static_cast<char>(38));
    msg.append(static_cast<char>(valueLsb & 0x7F));

    return msg;
}

// Allen & Heath scene recall over MIDI is a Bank Select (CC0) followed by a
// Program Change, per the A&H MIDI Protocol. sceneNumber is the 1-based scene as
// shown on the console; MIDI values are offset by -1, and scenes split into
// banks of 128 (1-128 -> bank 0, 129-256 -> bank 1, 257-300 -> bank 2; GLD has
// a fourth bank for 385-500).
QByteArray AllenHeathMidiProtocol::buildSceneRecall(int sceneNumber) {
    const int index = sceneNumber - 1;
    if (index < 0)
        return {}; // "None" / unset

    const int bank = index / 128;
    const int program = index % 128;
    const int channel = m_midiChannel & 0x0F;

    QByteArray msg;
    msg.append(static_cast<char>(0xB0 | channel)); // Control Change
    msg.append(static_cast<char>(0x00));           // CC 0 = Bank Select MSB
    msg.append(static_cast<char>(bank & 0x7F));
    msg.append(static_cast<char>(0xC0 | channel)); // Program Change
    msg.append(static_cast<char>(program & 0x7F));

    return msg;
}

QByteArray AllenHeathMidiProtocol::buildControlChange(int cc, int value) const {
    QByteArray msg;
    msg.append(static_cast<char>(0xB0 | (m_midiChannel & 0x0F)));
    msg.append(static_cast<char>(cc & 0x7F));
    msg.append(static_cast<char>(value & 0x7F));
    return msg;
}

void AllenHeathMidiProtocol::parseMidiData(const QByteArray& data) {
    m_receiveBuffer.append(data);

    while (!m_receiveBuffer.isEmpty()) {
        const unsigned char status = static_cast<unsigned char>(m_receiveBuffer[0]);

        if ((status & 0x80) == 0) {
            // a data byte with no status: running status is not used on this
            // link, so it is noise or the tail of something already consumed
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        if (status == 0xF0) {
            const int endPos = m_receiveBuffer.indexOf(static_cast<char>(0xF7));
            if (endPos < 0)
                break; // incomplete

            QByteArray sysex = m_receiveBuffer.left(endPos + 1);
            m_receiveBuffer.remove(0, endPos + 1);

            processSysEx(sysex);
            continue;
        }

        if (status >= 0xF8) {
            // system real-time is a single byte: Active Sensing (FE), which Qu
            // sends every 300 ms, clock, start/stop. Nothing to decode, and
            // treating it as a 3-byte message would shift everything after it.
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        // message length from the status byte
        int len = 3;
        if (status >= 0xF1) {
            // system common: F1 quarter-frame, F3 song select = 2; F2 song
            // position = 3; F4/F5 undefined, F6 tune request, stray F7 = 1
            len = (status == 0xF1 || status == 0xF3) ? 2 : (status == 0xF2 ? 3 : 1);
        } else if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
            len = 2; // program change, channel pressure
        }

        // collect the data bytes; real-time bytes may be interleaved anywhere,
        // and a fresh status byte means the message was cut short
        unsigned char data[2] = {0, 0};
        int have = 0;
        int pos = 1;
        bool truncated = false;
        while (have < len - 1 && pos < m_receiveBuffer.size()) {
            const unsigned char b = static_cast<unsigned char>(m_receiveBuffer[pos]);
            if (b >= 0xF8) {
                ++pos;
                continue;
            }
            if (b & 0x80) {
                truncated = true;
                break;
            }
            data[have++] = b;
            ++pos;
        }

        if (truncated) {
            m_receiveBuffer.remove(0, pos); // drop the partial message, keep the new status
            continue;
        }
        if (have < len - 1)
            break; // the rest is still in flight

        m_receiveBuffer.remove(0, pos);

        const int channel = status & 0x0F;
        if ((status & 0xF0) == 0xB0) {
            processControlChange(channel, data[0], data[1]);
        } else if ((status & 0xF0) == 0x90 && channel == m_midiChannel) {
            handleNoteOn(data[0], data[1]);
        }
    }
}

void AllenHeathMidiProtocol::processControlChange(int channel, int cc, int value) {
    if (channel != m_midiChannel) {
        return; // another channel: MIDI strips, DAW control, not the desk
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_nrpnState.timestamp > 0 && (now - m_nrpnState.timestamp) > NRPN_TIMEOUT_MS) {
        finishNrpn();
    }

    // NRPN message sequence: CC 99 (MSB), CC 98 (LSB), CC 6 (data MSB), CC 38 (data LSB)
    switch (cc) {
    case 99:
        // a new sequence closes whatever three-message one is still open
        finishNrpn();
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
            // GLD faders stop here; wait briefly for a data LSB before deciding
            m_nrpnFlushTimer.start();
        }
        break;

    case 38:
        if (m_nrpnState.channel == channel && m_nrpnState.dataMsb >= 0) {
            m_nrpnState.dataLsb = value;
            m_nrpnState.timestamp = now;
            finishNrpn();
        }
        break;

    default:
        break;
    }
}

void AllenHeathMidiProtocol::onNrpnFlush() { finishNrpn(); }

void AllenHeathMidiProtocol::finishNrpn() {
    m_nrpnFlushTimer.stop();
    if (!m_nrpnState.isComplete()) {
        m_nrpnState.reset();
        return;
    }
    const NRPNState done = m_nrpnState;
    m_nrpnState.reset();
    handleNrpn(done.nrpnMsb, done.nrpnLsb, done.dataMsb, done.dataLsb);
}

void AllenHeathMidiProtocol::reportParameter(const QString& path, const QVariant& value) {
    m_parameterCache[path] = value;
    emit parameterChanged(path, value);
}

void AllenHeathMidiProtocol::handleNrpn(int msb, int lsb, int dataMsb, int dataLsb) {
    // SQ map (SQ Iss5 / Qu-5/6/7 Iss2 reference tables)
    const quint16 value14 = static_cast<quint16>((dataMsb << 7) | (dataLsb >= 0 ? dataLsb : 0));

    if (msb == CH_LEVEL_TO_LR_MSB && lsb < m_capabilities.inputChannels) {
        const int channel = lsb + 1;
        const double dB = decodeLevel14(value14);
        reportParameter(QString("/ch/%1/fader").arg(channel), dB);
        emit channelFaderChanged(channel, dB);
    } else if (msb == DCA_LEVEL_MSB && lsb >= DCA_LEVEL_LSB_BASE &&
               lsb < DCA_LEVEL_LSB_BASE + m_capabilities.dcaCount) {
        reportParameter(dcaFaderPath(lsb - DCA_LEVEL_LSB_BASE + 1), decodeLevel14(value14));
    } else if (msb == CH_MUTE_MSB && lsb < m_capabilities.inputChannels) {
        reportParameter(QString("/ch/%1/mute").arg(lsb + 1), dataLsb == 0x01);
    } else if (msb == DCA_MUTE_MSB && lsb < m_capabilities.dcaCount) {
        reportParameter(dcaMutePath(lsb + 1), dataLsb == 0x01);
    }
}

void AllenHeathMidiProtocol::handleNoteOn(int note, int velocity) {
    Q_UNUSED(note);
    Q_UNUSED(velocity);
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
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void AllenHeathMidiProtocol::onDataReceived(const QByteArray& data) { parseMidiData(data); }

void AllenHeathMidiProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        // Active Sensing (FE) as keep-alive: a Qu closes a link that has been
        // silent for 12 s once it has seen one of these, so keep them coming
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
