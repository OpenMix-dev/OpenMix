#include "YamahaProtocol.h"
#include "../../core/Cue.h"
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace OpenMix {

YamahaProtocol::YamahaProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps) {

    QObject::connect(&m_transport, &TcpTransport::connected, this,
                     &YamahaProtocol::onTransportConnected);
    QObject::connect(&m_transport, &TcpTransport::disconnected, this,
                     &YamahaProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &TcpTransport::connectionError, this,
                     &YamahaProtocol::onTransportError);
    QObject::connect(&m_transport, &TcpTransport::connectionLost, this,
                     &YamahaProtocol::onTransportConnectionLost);
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     &YamahaProtocol::onDataReceived);
    QObject::connect(&m_transport, &TcpTransport::reconnecting, this,
                     &YamahaProtocol::onReconnecting);

    m_keepAliveTimer.setInterval(KEEPALIVE_INTERVAL);
    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &YamahaProtocol::onKeepAliveTimeout);

    m_requestTimeoutTimer.setInterval(500);
    QObject::connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
                     &YamahaProtocol::onRequestTimeoutCheck);
}

YamahaProtocol::~YamahaProtocol() { disconnect(); }

// --------------------------------------------------------------------------
// Pure command framing (no I/O, no state) -- unit tested for exact strings.
// --------------------------------------------------------------------------

QByteArray YamahaProtocol::scpSet(const QString& address, int idx1, int idx2, int value) {
    return QStringLiteral("set %1 %2 %3 %4\n")
        .arg(address)
        .arg(idx1)
        .arg(idx2)
        .arg(value)
        .toUtf8();
}

QByteArray YamahaProtocol::scpSet(const QString& address, int idx1, int idx2,
                                  const QString& value) {
    return QStringLiteral("set %1 %2 %3 \"%4\"\n")
        .arg(address)
        .arg(idx1)
        .arg(idx2)
        .arg(value)
        .toUtf8();
}

QByteArray YamahaProtocol::scpGet(const QString& address, int idx1, int idx2) {
    return QStringLiteral("get %1 %2 %3\n").arg(address).arg(idx1).arg(idx2).toUtf8();
}

QByteArray YamahaProtocol::scpSceneRecall(int sceneNumber) {
    return QStringLiteral("ssrecall_ex %1 %2\n").arg(SceneLib).arg(sceneNumber).toUtf8();
}

// --------------------------------------------------------------------------
// Pure value scaling.
// --------------------------------------------------------------------------

int YamahaProtocol::faderLevelToScp(double level) {
    if (level <= 0.0)
        return FADER_NEG_INF; // -inf
    if (level >= 1.0)
        return FADER_MAX_CENTIDB; // +10 dB

    // Two-segment, linear-in-dB approximation of the Yamaha fader law:
    //   0.00 .. 0.75 -> -138 dB .. 0 dB   (throw up to unity)
    //   0.75 .. 1.00 ->    0 dB .. +10 dB (top of throw)
    // The real console taper is finer near unity; this is a documented approx.
    double db;
    if (level >= 0.75)
        db = (level - 0.75) / 0.25 * 10.0;
    else
        db = (level / 0.75) * 138.0 - 138.0;

    int centi = static_cast<int>(std::lround(db * 100.0));
    return std::clamp(centi, FADER_MIN_CENTIDB, FADER_MAX_CENTIDB);
}

int YamahaProtocol::dbToCentiDb(double db) { return static_cast<int>(std::lround(db * 100.0)); }

int YamahaProtocol::hzToScpFreq(double hz) { return static_cast<int>(std::lround(hz * 10.0)); }

int YamahaProtocol::qToScp(double q) { return static_cast<int>(std::lround(q * 1000.0)); }

int YamahaProtocol::preampGainToScp(double gainDb) const {
    // CL/QL/Rivage: head-amp gain in centi-dB, range -6 dB..+66 dB.
    return std::clamp(dbToCentiDb(gainDb), -600, 6600);
}

// --------------------------------------------------------------------------
// Semantic command builders.
// --------------------------------------------------------------------------

QByteArray YamahaProtocol::buildChannelFader(int ch, double level) const {
    return scpSet(AddrFaderLevel, ch, 0, faderLevelToScp(level));
}

QByteArray YamahaProtocol::buildChannelMute(int ch, bool muted) const {
    // Fader/On: 1 = channel ON (unmuted), 0 = muted.
    return scpSet(AddrFaderOn, ch, 0, muted ? 0 : 1);
}

QByteArray YamahaProtocol::buildChannelPreamp(int ch, double gainDb) const {
    return scpSet(AddrHaGain, ch, 0, preampGainToScp(gainDb));
}

QByteArray YamahaProtocol::buildChannelHpfOn(int ch, bool on) const {
    return scpSet(AddrHpfOn, ch, 0, on ? 1 : 0);
}

QByteArray YamahaProtocol::buildChannelHpfFreq(int ch, double freqHz) const {
    return scpSet(AddrHpfFreq, ch, 0, hzToScpFreq(freqHz));
}

QByteArray YamahaProtocol::buildChannelEqOn(int ch, bool on) const {
    return scpSet(AddrEqOn, ch, 0, on ? 1 : 0);
}

QByteArray YamahaProtocol::buildChannelEqBandBypass(int ch, int band, bool on) const {
    // Band/Bypass: 1 = bypassed, 0 = active, so "on" inverts.
    return scpSet(AddrEqBandBypass, ch, band, on ? 0 : 1);
}

QByteArray YamahaProtocol::buildChannelEqBandFreq(int ch, int band, double freqHz) const {
    return scpSet(AddrEqBandFreq, ch, band, hzToScpFreq(freqHz));
}

QByteArray YamahaProtocol::buildChannelEqBandGain(int ch, int band, double gainDb) const {
    return scpSet(AddrEqBandGain, ch, band, std::clamp(dbToCentiDb(gainDb), -1800, 1800));
}

QByteArray YamahaProtocol::buildChannelEqBandQ(int ch, int band, double q) const {
    return scpSet(AddrEqBandQ, ch, band, std::clamp(qToScp(q), 100, 16000));
}

QByteArray YamahaProtocol::buildChannelDynamicsThreshold(int ch, double thresholdDb) const {
    // Dyna2 (compressor) threshold is deci-dB, range -54 dB..0 dB.
    int deci = static_cast<int>(std::lround(thresholdDb * 10.0));
    return scpSet(AddrDynaThreshold, ch, 0, std::clamp(deci, -540, 0));
}

QByteArray YamahaProtocol::buildChannelName(int ch, const QString& name) const {
    return scpSet(AddrChannelName, ch, 0, name);
}

QByteArray YamahaProtocol::buildChannelColor(int ch, int color) const {
    return scpSet(AddrChannelColor, ch, 0, color);
}

// --------------------------------------------------------------------------
// Semantic setters (send when connected, and cache).
// --------------------------------------------------------------------------

// public setters take a 1-based channel (MixerProtocol contract); the SCP
// builders use the 0-based input-channel index.
void YamahaProtocol::setChannelFader(int ch, double level) {
    sendCommand(buildChannelFader(ch - 1, level));
}

void YamahaProtocol::setChannelMute(int ch, bool muted) {
    sendCommand(buildChannelMute(ch - 1, muted));
}

void YamahaProtocol::setChannelPreamp(int ch, double gainDb) {
    sendCommand(buildChannelPreamp(ch - 1, gainDb));
}

void YamahaProtocol::setChannelHpf(int ch, bool on, double freqHz) {
    sendCommand(buildChannelHpfOn(ch - 1, on));
    if (freqHz > 0.0)
        sendCommand(buildChannelHpfFreq(ch - 1, freqHz));
}

void YamahaProtocol::setChannelEqOn(int ch, bool on) { sendCommand(buildChannelEqOn(ch - 1, on)); }

void YamahaProtocol::setChannelEqBand(int ch, int band, bool on, int type, double freqHz,
                                      double gainDb, double q) {
    // Yamaha PEQ bands are parametric; per-band shelf "type" is toggled by the
    // console rather than addressed as a band node over SCP, so `type` is not
    // transmitted by this driver.
    Q_UNUSED(type);
    sendCommand(buildChannelEqBandBypass(ch - 1, band, on));
    sendCommand(buildChannelEqBandFreq(ch - 1, band, freqHz));
    sendCommand(buildChannelEqBandGain(ch - 1, band, gainDb));
    sendCommand(buildChannelEqBandQ(ch - 1, band, q));
}

void YamahaProtocol::setChannelDynamics(int ch, bool on, double thresholdDb, double ratio,
                                        double attackMs, double releaseMs, double makeupDb) {
    // SCP exposes the compressor (Dyna2) threshold; on/off, ratio, attack,
    // release and makeup are not addressable via this driver.
    Q_UNUSED(on);
    Q_UNUSED(ratio);
    Q_UNUSED(attackMs);
    Q_UNUSED(releaseMs);
    Q_UNUSED(makeupDb);
    sendCommand(buildChannelDynamicsThreshold(ch - 1, thresholdDb));
}

void YamahaProtocol::setChannelName(int ch, const QString& name) {
    sendCommand(buildChannelName(ch - 1, name));
}

void YamahaProtocol::setChannelColor(int ch, int color) {
    sendCommand(buildChannelColor(ch - 1, color));
}

// --------------------------------------------------------------------------
// Connection lifecycle.
// --------------------------------------------------------------------------

bool YamahaProtocol::connect(const QString& host, int port) {
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port > 0 ? port : SCP_PORT; // SCP is always 49280
    m_rxBuffer.clear();

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(m_host).arg(m_port));

    m_transport.setReconnectEnabled(true);
    return m_transport.connect(m_host, m_port); // async; result via signals
}

void YamahaProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_requestTimeoutTimer.stop();

    m_transport.setReconnectEnabled(false);
    m_transport.disconnect();

    m_rxBuffer.clear();
    m_parameterCache.clear();
    m_pendingRequests.clear();
    m_latencyMs = 0;
    m_lastResponseTime = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void YamahaProtocol::onTransportConnected() {
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

    m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();
    m_keepAliveTimer.start();
    m_requestTimeoutTimer.start();

    // Identify the console; doubles as an initial liveness probe.  Connection
    // itself is established the moment the socket connects (per SCP practice).
    sendCommand(QByteArray("devinfo productname\n"));

    emit connected();
}

void YamahaProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    m_requestTimeoutTimer.stop();
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void YamahaProtocol::onTransportError(const QString& error) {
    setStatus("Connection error: " + error);
    emit connectionError(error);
}

void YamahaProtocol::onTransportConnectionLost() {
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost - reconnecting...");
    emit connectionLost();
}

void YamahaProtocol::onReconnecting(int attempt, int maxAttempts) {
    setConnectionState(ConnectionState::Reconnecting);
    setStatus(QString("Reconnecting (attempt %1/%2)...").arg(attempt).arg(maxAttempts));
}

// --------------------------------------------------------------------------
// Incoming data.
// --------------------------------------------------------------------------

void YamahaProtocol::onDataReceived(const QByteArray& data) {
    m_rxBuffer.append(data);

    int nl;
    while ((nl = m_rxBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_rxBuffer.left(nl);
        m_rxBuffer.remove(0, nl + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        parseScpLine(line);
    }

    if (m_rxBuffer.size() > MAX_RX_BUFFER)
        m_rxBuffer.clear(); // guard against a peer that never sends LF
}

void YamahaProtocol::parseScpLine(const QByteArray& rawLine) {
    const QString line = QString::fromUtf8(rawLine).trimmed();
    if (line.isEmpty())
        return;

    m_lastResponseTime = QDateTime::currentMSecsSinceEpoch();

    // Tokenize on whitespace, but keep "quoted strings" as single tokens.
    QStringList tokens;
    {
        bool inQuotes = false;
        QString cur;
        for (const QChar c : line) {
            if (c == '"') {
                inQuotes = !inQuotes;
                continue;
            }
            if (c.isSpace() && !inQuotes) {
                if (!cur.isEmpty()) {
                    tokens.append(cur);
                    cur.clear();
                }
            } else {
                cur.append(c);
            }
        }
        if (!cur.isEmpty())
            tokens.append(cur);
    }
    if (tokens.isEmpty())
        return;

    const QString& status = tokens.first();
    // Per-command "ERROR" lines are routine (e.g. querying an unsupported node)
    // and are ignored here rather than surfaced as connection errors.
    if (status != "OK" && status != "OKm" && status != "NOTIFY" && status != "NOTIFYm")
        return;

    // Device identification: e.g.  OK devinfo productname "CL5"
    if (tokens.size() >= 4 && tokens.at(1) == "devinfo" && tokens.at(2) == "productname") {
        setStatus(QString("Connected to %1 (%2:%3)").arg(tokens.at(3)).arg(m_host).arg(m_port));
        return;
    }

    // Find the parameter address (the "MIXER:..." token).
    int ai = -1;
    for (int i = 1; i < tokens.size(); ++i) {
        if (tokens.at(i).startsWith("MIXER:")) {
            ai = i;
            break;
        }
    }
    if (ai < 0)
        return;

    const QString address = tokens.at(ai);

    // Value: for "set"/"get" it follows address + idx1 + idx2; for scene-style
    // lines (ssrecall_ex MIXER:Lib/Scene <n>) it follows the address directly.
    QVariant value;
    auto parseToken = [](const QString& t) -> QVariant {
        bool ok = false;
        int iv = t.toInt(&ok);
        return ok ? QVariant(iv) : QVariant(t);
    };
    if (tokens.size() > ai + 3)
        value = parseToken(tokens.at(ai + 3));
    else if (tokens.size() > ai + 1)
        value = parseToken(tokens.at(ai + 1));

    if (m_pendingRequests.contains(address)) {
        const YamahaPendingRequest req = m_pendingRequests.take(address);
        updateLatency(m_lastResponseTime - req.sentTime);
        if (req.callback)
            req.callback(address, value, true);
    }

    if (value.isValid()) {
        m_parameterCache[address] = value;
        emit parameterChanged(address, value);
    }
}

// --------------------------------------------------------------------------
// Parameter API.
// --------------------------------------------------------------------------

void YamahaProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // `path` is treated as a raw SCP target ("<address> <idx1> <idx2>", or just
    // an address for which 0 0 is assumed).
    const QString target = path.contains(' ') ? path : (path + " 0 0");

    QString valueStr;
    switch (value.typeId()) {
    case QMetaType::Bool:
        valueStr = value.toBool() ? "1" : "0";
        break;
    case QMetaType::Int:
    case QMetaType::LongLong:
        valueStr = QString::number(value.toLongLong());
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        valueStr = QString::number(std::lround(value.toDouble()));
        break;
    case QMetaType::QString:
        valueStr = "\"" + value.toString() + "\"";
        break;
    default:
        valueStr = QString::number(std::lround(value.toDouble()));
        break;
    }

    sendCommand(("set " + target + " " + valueStr + "\n").toUtf8());
    m_parameterCache[path] = value;
}

QVariant YamahaProtocol::getParameter(const QString& path) { return m_parameterCache.value(path); }

void YamahaProtocol::requestParameter(const QString& path) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    const QString target = path.contains(' ') ? path : (path + " 0 0");
    const QString address = path.section(' ', 0, 0);

    YamahaPendingRequest req;
    req.address = address;
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = nullptr;
    m_pendingRequests[address] = req;

    sendCommand(("get " + target + "\n").toUtf8());
}

void YamahaProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_connectionState != ConnectionState::Connected) {
        if (callback)
            callback(path, QVariant(), false);
        return;
    }

    const QString target = path.contains(' ') ? path : (path + " 0 0");
    const QString address = path.section(' ', 0, 0);

    YamahaPendingRequest req;
    req.address = address;
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = std::move(callback);
    m_pendingRequests[address] = req;

    sendCommand(("get " + target + "\n").toUtf8());
}

void YamahaProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    const QJsonObject params = cue.parameters();
    for (const auto& [path, value] : params.asKeyValueRange())
        sendParameter(path.toString(), value.toVariant());
}

void YamahaProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    sendCommand(scpSceneRecall(sceneNumber));
    emit sceneChanged(sceneNumber);
}

void YamahaProtocol::refresh() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // Re-query input-channel fader level / on so cached state matches console.
    const int n = std::max(0, m_capabilities.inputChannels);
    for (int ch = 0; ch < n; ++ch) {
        sendCommand(scpGet(AddrFaderLevel, ch, 0));
        sendCommand(scpGet(AddrFaderOn, ch, 0));
    }
}

// --------------------------------------------------------------------------
// Timers / housekeeping.
// --------------------------------------------------------------------------

void YamahaProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected)
        sendCommand(QByteArray("devinfo productname\n"));
}

void YamahaProtocol::onRequestTimeoutCheck() {
    if (m_pendingRequests.isEmpty())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList timedOut;
    for (const auto& [address, req] : m_pendingRequests.asKeyValueRange()) {
        if (now - req.sentTime > REQUEST_TIMEOUT_MS)
            timedOut.append(address);
    }

    for (const QString& address : timedOut) {
        const YamahaPendingRequest req = m_pendingRequests.take(address);
        if (req.callback)
            req.callback(address, QVariant(), false);
        emit requestTimeout(address);
    }
}

void YamahaProtocol::sendCommand(const QByteArray& cmd) {
    if (cmd.isEmpty() || !m_transport.isConnected())
        return;
    m_transport.send(cmd);
}

void YamahaProtocol::updateLatency(qint64 roundTripMs) {
    if (roundTripMs < 0)
        return;
    // Simple exponential moving average.
    const int sample = static_cast<int>(roundTripMs);
    const int next = m_latencyMs == 0 ? sample : (m_latencyMs * 3 + sample) / 4;
    if (next != m_latencyMs) {
        m_latencyMs = next;
        emit latencyChanged(m_latencyMs);
    }
}

void YamahaProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void YamahaProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
