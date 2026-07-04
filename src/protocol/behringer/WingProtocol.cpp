#include "WingProtocol.h"
#include "../../core/Cue.h"
#include <QDateTime>
#include <algorithm>

namespace OpenMix {

WingProtocol::WingProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort; // 2223

    initializeSnapshotParams();

    // transport signals
    QObject::connect(&m_transport, &OscTransport::connected, this,
                     &WingProtocol::onTransportConnected);
    QObject::connect(&m_transport, &OscTransport::disconnected, this,
                     &WingProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &OscTransport::connectionError, this,
                     &WingProtocol::onTransportError);
    QObject::connect(&m_transport, &OscTransport::messageReceived, this,
                     &WingProtocol::onMessageReceived);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this, &WingProtocol::onKeepAliveTimeout);

    m_connectionTimer.setSingleShot(true);
    QObject::connect(&m_connectionTimer, &QTimer::timeout, this,
                     &WingProtocol::onConnectionTimeout);

    m_requestTimeoutTimer.setInterval(500);
    QObject::connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
                     &WingProtocol::onRequestTimeoutCheck);

    m_reconnectTimer.setSingleShot(true);
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this, &WingProtocol::onReconnectAttempt);
}

WingProtocol::~WingProtocol() { disconnect(); }

void WingProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // WING has up to 48 input channels
    for (int i = 1; i <= m_capabilities.inputChannels && i <= 48; ++i) {
        QString chPrefix = QString("/ch/%1").arg(i);
        m_snapshotParams.append(chPrefix + "/fader");
        m_snapshotParams.append(chPrefix + "/mute");

        // EQ params
        if (m_capabilities.supportsChannelEQ) {
            m_snapshotParams.append(chPrefix + "/eq/on");
            for (int band = 1; band <= m_capabilities.eqBandsPerChannel; ++band) {
                QString bandPrefix = QString("%1/eq/%2").arg(chPrefix).arg(band);
                m_snapshotParams.append(bandPrefix + "/type");
                m_snapshotParams.append(bandPrefix + "/f");
                m_snapshotParams.append(bandPrefix + "/g");
                m_snapshotParams.append(bandPrefix + "/q");
            }
        }

        // effect send params
        if (m_capabilities.supportsEffectSends) {
            int sends = std::min(m_capabilities.effectSendBuses, 16);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix = QString("%1/send/%2").arg(chPrefix).arg(send);
                m_snapshotParams.append(sendPrefix + "/level");
                m_snapshotParams.append(sendPrefix + "/on");
            }
        }
    }

    // WING has up to 16 buses
    for (int i = 1; i <= m_capabilities.mixBuses && i <= 16; ++i) {
        QString busPrefix = QString("/bus/%1").arg(i);
        m_snapshotParams.append(busPrefix + "/fader");
        m_snapshotParams.append(busPrefix + "/mute");

        // bus EQ params
        if (m_capabilities.supportsChannelEQ) {
            m_snapshotParams.append(busPrefix + "/eq/on");
            for (int band = 1; band <= m_capabilities.eqBandsPerChannel; ++band) {
                QString bandPrefix = QString("%1/eq/%2").arg(busPrefix).arg(band);
                m_snapshotParams.append(bandPrefix + "/type");
                m_snapshotParams.append(bandPrefix + "/f");
                m_snapshotParams.append(bandPrefix + "/g");
                m_snapshotParams.append(bandPrefix + "/q");
            }
        }
    }

    // main outputs
    m_snapshotParams.append("/main/lr/fader");
    m_snapshotParams.append("/main/lr/mute");

    // main EQ
    if (m_capabilities.supportsChannelEQ) {
        m_snapshotParams.append("/main/lr/eq/on");
        for (int band = 1; band <= m_capabilities.eqBandsPerChannel; ++band) {
            QString bandPrefix = QString("/main/lr/eq/%1").arg(band);
            m_snapshotParams.append(bandPrefix + "/type");
            m_snapshotParams.append(bandPrefix + "/f");
            m_snapshotParams.append(bandPrefix + "/g");
            m_snapshotParams.append(bandPrefix + "/q");
        }
    }

    // WING has up to 24 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 24; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

bool WingProtocol::connect(const QString& host, int port) {
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port;
    m_reconnectAttempts = 0;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(port));

    if (!m_transport.connect(host, port)) {
        setStatus("Failed to initialize transport");
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to initialize transport");
        return false;
    }

    // request device info to verify connection
    m_waitingForInfo = true;
    m_transport.send("/$info");

    m_connectionTimer.start(m_connectionTimeoutMs);

    return true;
}

void WingProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_reconnectTimer.stop();
    m_requestTimeoutTimer.stop();

    m_transport.disconnect();

    m_parameterCache.clear();
    m_pendingRequests.clear();
    m_latencyHistory.clear();
    m_latencyMs = 0;
    m_reconnectAttempts = 0;
    m_waitingForInfo = false;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void WingProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    m_transport.send(path, value);
    m_parameterCache[path] = value;
}

QVariant WingProtocol::getParameter(const QString& path) { return m_parameterCache.value(path); }

void WingProtocol::requestParameter(const QString& path) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    WingPendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = nullptr;
    m_pendingRequests[path] = req;

    m_transport.send(path);
}

void WingProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_connectionState != ConnectionState::Connected) {
        if (callback) {
            callback(path, QVariant(), false);
        }
        return;
    }

    WingPendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = callback;
    m_pendingRequests[path] = req;

    m_transport.send(path);
}

void WingProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (const auto& [path, value] : params.asKeyValueRange()) {
        sendParameter(path.toString(), value.toVariant());
    }
}

void WingProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // WING scene recall via OSC
    m_transport.send("/action/scenes/recall", sceneNumber);
}

void WingProtocol::recallSnippet(int snippetNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // WING snippet recall
    m_transport.send("/-action/gosnippet", snippetNumber);
}

namespace {
QString wingChannel(int channel) { return QString("/ch/%1").arg(channel); }

// WING faders carry real-world dB; map a normalized 0..1 level onto the exact
// X32/WING fader law (piecewise-linear in dB, per the Maillot/WING references):
//   0.0000-0.0625 -> -inf..-60, 0.0625-0.25 -> -60..-30,
//   0.25-0.5      -> -30..-10,  0.5-1.0      -> -10..+10  (0.75 = 0 dB).
float wingFaderDb(double level) {
    level = std::clamp(level, 0.0, 1.0);
    if (level <= 0.0)
        return -144.0f; // -inf floor
    if (level < 0.0625)
        return static_cast<float>(-60.0 - (0.0625 - level) / 0.0625 * 84.0);
    if (level < 0.25)
        return static_cast<float>(level * 160.0 - 70.0);
    if (level < 0.5)
        return static_cast<float>(level * 80.0 - 50.0);
    return static_cast<float>(level * 40.0 - 30.0);
}

// WING STD EQ exposes named bands l,1,2,3,4,h (not numbered 1..N)
QString wingEqBand(int band) {
    static const char* tokens[] = {"l", "1", "2", "3", "4", "h"};
    int idx = std::clamp(band - 1, 0, 5);
    return tokens[idx];
}
} // namespace

void WingProtocol::setChannelFader(int channel, double level) {
    sendParameter(wingChannel(channel) + "/fdr", wingFaderDb(level));
}

void WingProtocol::setChannelMute(int channel, bool muted) {
    // WING /mute polarity is the opposite of X32: 1 = muted, 0 = unmuted
    sendParameter(wingChannel(channel) + "/mute", muted ? 1 : 0);
}

void WingProtocol::setChannelPreamp(int channel, double gainDb) {
    // per-channel head-amp gain (real dB), follows the patched source
    sendParameter(wingChannel(channel) + "/in/set/$g", static_cast<float>(gainDb));
}

void WingProtocol::setChannelHpf(int channel, bool on, double freqHz) {
    // WING's HPF is the channel low-cut filter
    const QString ch = wingChannel(channel);
    sendParameter(ch + "/flt/lc", on ? 1 : 0);
    sendParameter(ch + "/flt/lcf", static_cast<float>(freqHz));
}

void WingProtocol::setChannelEqOn(int channel, bool on) {
    sendParameter(wingChannel(channel) + "/eq/on", on ? 1 : 0);
}

void WingProtocol::setChannelEqBand(int channel, int band, bool /*on*/, int /*type*/, double freqHz,
                                    double gainDb, double q) {
    // WING named bands carry real-world values: /ch/N/eq/<tok>{g,f,q}
    const QString prefix = wingChannel(channel) + "/eq/" + wingEqBand(band);
    sendParameter(prefix + "g", static_cast<float>(gainDb));
    sendParameter(prefix + "f", static_cast<float>(freqHz));
    sendParameter(prefix + "q", static_cast<float>(q));
}

void WingProtocol::setChannelDynamics(int channel, bool on, double thresholdDb, double ratio,
                                      double attackMs, double releaseMs, double makeupDb) {
    const QString ch = wingChannel(channel);
    sendParameter(ch + "/dyn/on", on ? 1 : 0);
    sendParameter(ch + "/dyn/thr", static_cast<float>(thresholdDb));
    sendParameter(ch + "/dyn/ratio", static_cast<float>(ratio));
    sendParameter(ch + "/dyn/att", static_cast<float>(attackMs));
    sendParameter(ch + "/dyn/rel", static_cast<float>(releaseMs));
    sendParameter(ch + "/dyn/gain", static_cast<float>(makeupDb));
}

void WingProtocol::setChannelName(int channel, const QString& name) {
    // WING channel name node (best-effort; console truncates to display width)
    sendParameter(wingChannel(channel) + "/$name", name);
}

void WingProtocol::setChannelColor(int channel, int color) {
    // WING channel color index (best-effort; palette differs from X32)
    sendParameter(wingChannel(channel) + "/col", color);
}

void WingProtocol::refresh() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
    }
}

void WingProtocol::onTransportConnected() {
    // transport connected, wait for info response
}

void WingProtocol::onTransportDisconnected() {
    if (m_connectionState == ConnectionState::Connected) {
        emit connectionLost();
        startReconnection();
    }
}

void WingProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);

    if (m_connectionState == ConnectionState::Connecting) {
        startReconnection();
    } else if (m_connectionState == ConnectionState::Reconnecting) {
        if (m_reconnectAttempts >= m_maxReconnectAttempts) {
            setStatus("Reconnection failed - max attempts reached");
            setConnectionState(ConnectionState::Disconnected);
        } else {
            int shift = std::min(m_reconnectAttempts, 10);
            int delay = std::min(m_reconnectDelayMs * (1 << shift), 30000);
            m_reconnectTimer.start(delay);
        }
    }
}

void WingProtocol::onMessageReceived(const QString& path, const QVariant& value) {
    processResponse(path, value);
}

void WingProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastResponseTime > 0 && (now - m_lastResponseTime) > (KEEPALIVE_INTERVAL * 3)) {
            setStatus("Connection lost - no response from mixer");
            emit connectionLost();
            startReconnection();
            return;
        }

        // send keep-alive
        m_transport.send("/$xremote");
    }
}

void WingProtocol::onConnectionTimeout() {
    if (m_connectionState == ConnectionState::Connecting) {
        setStatus("Connection timeout");
        emit connectionError("Connection timeout - no response from mixer");
        startReconnection();
    } else if (m_connectionState == ConnectionState::Reconnecting) {
        if (m_reconnectAttempts >= m_maxReconnectAttempts) {
            setStatus("Reconnection failed - max attempts reached");
            setConnectionState(ConnectionState::Disconnected);
            emit connectionError("Failed to reconnect after maximum attempts");
        } else {
            int shift = std::min(m_reconnectAttempts, 10);
            int delay = std::min(m_reconnectDelayMs * (1 << shift), 30000);
            m_reconnectTimer.start(delay);
        }
    }
}

void WingProtocol::onRequestTimeoutCheck() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QDateTime now = QDateTime::currentDateTime();
    QStringList timedOut;

    for (const auto& [path, req] : m_pendingRequests.asKeyValueRange()) {
        if (req.timestamp.msecsTo(now) > m_requestTimeoutMs) {
            timedOut.append(path);
        }
    }

    for (const QString& path : timedOut) {
        WingPendingRequest req = m_pendingRequests.take(path);
        if (req.callback) {
            req.callback(path, QVariant(), false);
        }
        emit requestTimeout(path);
    }
}

void WingProtocol::onReconnectAttempt() {
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        setStatus("Reconnection failed - max attempts reached");
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to reconnect after maximum attempts");
        return;
    }

    m_reconnectAttempts++;
    setStatus(QString("Reconnecting (attempt %1/%2)...")
                  .arg(m_reconnectAttempts)
                  .arg(m_maxReconnectAttempts));

    m_transport.disconnect();

    if (!m_transport.connect(m_host, m_port)) {
        int shift = std::min(m_reconnectAttempts - 1, 10);
        int delay = std::min(m_reconnectDelayMs * (1 << shift), 30000);
        m_reconnectTimer.start(delay);
        return;
    }

    m_waitingForInfo = true;
    m_transport.send("/$info");
    m_connectionTimer.start(m_connectionTimeoutMs);
}

void WingProtocol::processResponse(const QString& path, const QVariant& value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastResponseTime = now;

    if (path == "/$info" || path.startsWith("/$")) {
        handleInfoResponse(value);
        return;
    }

    if (m_pendingRequests.contains(path)) {
        WingPendingRequest req = m_pendingRequests.take(path);
        qint64 roundTrip = now - req.sentTime;
        updateLatency(roundTrip);

        if (req.callback) {
            req.callback(path, value, true);
        }
    }

    if (value.isValid()) {
        m_parameterCache[path] = value;
        emit parameterChanged(path, value);
    }
}

void WingProtocol::handleInfoResponse([[maybe_unused]] const QVariant& value) {
    m_connectionTimer.stop();
    m_waitingForInfo = false;

    if (m_connectionState == ConnectionState::Connecting ||
        m_connectionState == ConnectionState::Reconnecting) {
        setConnectionState(ConnectionState::Connected);
        setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

        m_keepAliveTimer.start(KEEPALIVE_INTERVAL);
        m_requestTimeoutTimer.start();
        m_reconnectAttempts = 0;

        // subscribe to updates
        m_transport.send("/$xremote");

        emit connected();
    }
}

void WingProtocol::startReconnection() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_requestTimeoutTimer.stop();

    setConnectionState(ConnectionState::Reconnecting);
    m_reconnectTimer.start(m_reconnectDelayMs);
}

void WingProtocol::updateLatency(qint64 roundTripMs) {
    m_latencyHistory.append(static_cast<int>(roundTripMs));

    while (m_latencyHistory.size() > LATENCY_HISTORY_SIZE) {
        m_latencyHistory.removeFirst();
    }

    int sum = 0;
    for (int lat : m_latencyHistory) {
        sum += lat;
    }
    int newLatency = sum / m_latencyHistory.size();

    if (newLatency != m_latencyMs) {
        m_latencyMs = newLatency;
        emit latencyChanged(m_latencyMs);
    }
}

void WingProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void WingProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
