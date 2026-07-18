#include "X32Protocol.h"
#include "../../core/Cue.h"
#include "../../core/LevelDb.h"
#include <QDateTime>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace OpenMix {

namespace {
constexpr int MAX_X32_INPUT_CHANNELS = 32;
constexpr int MAX_X32_EFFECT_SENDS = 16;
constexpr int MAX_X32_MIX_BUSES = 16;

// X32 OSC encodes most continuous params as a normalized float 0..1 mapped onto
// the parameter's real range. These helpers convert real-world units to that
// 0..1 value. The curves are close approximations of the console's mapping; the
// exact lookup tables can refine the constants without changing call sites.
QString x32Channel(int channel) { return QString("/ch/%1").arg(channel, 2, 10, QChar('0')); }

QString x32Bus(int bus) { return QString("/bus/%1").arg(bus, 2, 10, QChar('0')); }

float clampUnit(double v) { return static_cast<float>(std::clamp(v, 0.0, 1.0)); }

// X32 faders are a position, not a level: the console takes a 0..1 float and
// applies its own piecewise law (X32 OSC Remote Protocol, "level(float)"):
//   0..0.0625 = -oo/-90..-60 dB, 0.0625..0.25 = -60..-30,
//   0.25..0.5 = -30..-10,        0.5..1.0     = -10..+10.
float x32FaderFromDb(double dB) {
    if (dB <= NEG_INF_DB || dB <= -90.0) {
        return 0.0f;
    }
    double pos;
    if (dB >= -10.0) {
        pos = (dB + 30.0) / 40.0; // -10 .. +10 dB
    } else if (dB >= -30.0) {
        pos = (dB + 50.0) / 80.0; // -30 .. -10 dB
    } else if (dB >= -60.0) {
        pos = (dB + 70.0) / 160.0; // -60 .. -30 dB
    } else {
        pos = (dB + 90.0) / 480.0; // -90 .. -60 dB
    }
    return clampUnit(pos);
}

// the paths whose value is a fader position rather than a real-world unit
bool isFaderPath(const QString& path) {
    return path.endsWith("/mix/fader") || path.endsWith("/fader");
}

double x32DbFromFader(double position) {
    position = std::clamp(position, 0.0, 1.0);
    if (position <= 0.0) {
        return NEG_INF_DB;
    }
    if (position >= 0.5) {
        return position * 40.0 - 30.0;
    }
    if (position >= 0.25) {
        return position * 80.0 - 50.0;
    }
    if (position >= 0.0625) {
        return position * 160.0 - 70.0;
    }
    return position * 480.0 - 90.0;
}

float linNorm(double v, double lo, double hi) { return clampUnit((v - lo) / (hi - lo)); }

float logNorm(double v, double lo, double hi) {
    if (v <= lo)
        return 0.0f;
    if (v >= hi)
        return 1.0f;
    return static_cast<float>(std::log(v / lo) / std::log(hi / lo));
}

// X32 compressor ratio is an enum index into a fixed table
int x32RatioIndex(double ratio) {
    static constexpr std::array<double, 12> ratios = {1.1, 1.3, 1.5, 2.0,  2.5,  3.0,
                                                      4.0, 5.0, 7.0, 10.0, 20.0, 100.0};
    int best = 0;
    double bestDiff = 1e9;
    for (int i = 0; i < static_cast<int>(ratios.size()); ++i) {
        double diff = std::abs(ratios[i] - ratio);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = i;
        }
    }
    return best;
}
} // namespace

X32Protocol::X32Protocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort;
    initializeSnapshotParams();

    QObject::connect(&m_transport, &OscTransport::connected, this,
                     &X32Protocol::onTransportConnected);
    QObject::connect(&m_transport, &OscTransport::disconnected, this,
                     &X32Protocol::onTransportDisconnected);
    QObject::connect(&m_transport, &OscTransport::connectionError, this,
                     &X32Protocol::onTransportError);
    QObject::connect(&m_transport, &OscTransport::messageReceived, this,
                     &X32Protocol::onMessageReceived);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this, &X32Protocol::onKeepAliveTimeout);

    m_connectionTimer.setSingleShot(true);
    QObject::connect(&m_connectionTimer, &QTimer::timeout, this, &X32Protocol::onConnectionTimeout);

    m_requestTimeoutTimer.setInterval(500);
    QObject::connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
                     &X32Protocol::onRequestTimeoutCheck);

    m_reconnectTimer.setSingleShot(true);
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this, &X32Protocol::onReconnectAttempt);
}

X32Protocol::~X32Protocol() { disconnect(); }

void X32Protocol::initializeSnapshotParams() { rebuildSnapshotParams(); }

void X32Protocol::rebuildSnapshotParams() {
    m_snapshotParams.clear();

    // basic fader/mute parameters
    for (int i = 1; i <= m_capabilities.inputChannels && i <= MAX_X32_INPUT_CHANNELS; ++i) {
        QString chPrefix = QString("/ch/%1").arg(i, 2, 10, QChar('0'));
        m_snapshotParams.append(chPrefix + "/mix/fader");
        m_snapshotParams.append(chPrefix + "/mix/on");
        m_snapshotParams.append(chPrefix + "/grp/dca");

        // EQ parameters
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

        // effect send parameters (mix bus sends)
        if (m_capabilities.supportsEffectSends) {
            int sends = std::min(m_capabilities.effectSendBuses, MAX_X32_EFFECT_SENDS);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_snapshotParams.append(sendPrefix + "/level");
                m_snapshotParams.append(sendPrefix + "/on");
            }
        }
    }

    // mix bus parameters
    for (int i = 1; i <= m_capabilities.mixBuses && i <= MAX_X32_MIX_BUSES; ++i) {
        QString busPrefix = QString("/bus/%1").arg(i, 2, 10, QChar('0'));
        m_snapshotParams.append(busPrefix + "/mix/fader");
        m_snapshotParams.append(busPrefix + "/mix/on");
        m_snapshotParams.append(busPrefix + "/grp/dca");

        // bus EQ parameters
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

    // main stereo bus
    m_snapshotParams.append("/main/st/mix/fader");
    m_snapshotParams.append("/main/st/mix/on");

    // main stereo EQ
    if (m_capabilities.supportsChannelEQ) {
        m_snapshotParams.append("/main/st/eq/on");
        for (int band = 1; band <= m_capabilities.eqBandsPerChannel; ++band) {
            QString bandPrefix = QString("/main/st/eq/%1").arg(band);
            m_snapshotParams.append(bandPrefix + "/type");
            m_snapshotParams.append(bandPrefix + "/f");
            m_snapshotParams.append(bandPrefix + "/g");
            m_snapshotParams.append(bandPrefix + "/q");
        }
    }

    // DCA parameters
    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

bool X32Protocol::connect(const QString& host, int port) {
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

    m_waitingForXinfo = true;
    // the reference probes with /info, and some consoles answer only that; /xinfo
    // carries the richer payload, so ask for both and connect on whichever lands
    m_transport.send("/info");
    m_transport.send("/xinfo");
    m_connectionTimer.start(m_connectionTimeoutMs);

    return true;
}

void X32Protocol::disconnect() {
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
    m_waitingForXinfo = false;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void X32Protocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    m_transport.send(path, value);
    m_parameterCache[path] = isFaderPath(path) ? QVariant(x32DbFromFader(value.toDouble())) : value;
}

QVariant X32Protocol::getParameter(const QString& path) { return m_parameterCache.value(path); }

void X32Protocol::requestParameter(const QString& path) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    X32PendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = nullptr;
    m_pendingRequests[path] = req;

    m_transport.send(path);
}

void X32Protocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_connectionState != ConnectionState::Connected) {
        if (callback) {
            callback(path, QVariant(), false);
        }
        return;
    }

    X32PendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = callback;
    m_pendingRequests[path] = req;

    m_transport.send(path);
}

void X32Protocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (const auto& [path, value] : params.asKeyValueRange()) {
        sendParameter(path.toString(), value.toVariant());
    }
}

void X32Protocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // X32 scene recall: /-action/goscene followed by scene number (0-indexed)
    m_transport.send("/-action/goscene", sceneNumber);
}

void X32Protocol::recallSnippet(int snippetNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // X32 snippet recall: /-action/gosnippet followed by snippet number
    m_transport.send("/-action/gosnippet", snippetNumber);
}

void X32Protocol::setChannelFaderDb(int channel, double dB) {
    sendParameter(x32Channel(channel) + "/mix/fader", x32FaderFromDb(dB));
}

std::optional<double> X32Protocol::readChannelFader(int channel) {
    const QVariant value = getParameter(x32Channel(channel) + "/mix/fader");
    if (!value.isValid())
        return std::nullopt;
    return value.toDouble();
}

void X32Protocol::setChannelMute(int channel, bool muted) {
    // /mix/on: 1 = on (unmuted), 0 = muted
    sendParameter(x32Channel(channel) + "/mix/on", muted ? 0 : 1);
}

void X32Protocol::setChannelPreamp(int channel, double gainDb) {
    // per-channel digital trim, +/-18 dB (head-amp analog gain is patch-dependent)
    sendParameter(x32Channel(channel) + "/preamp/trim", linNorm(gainDb, -18.0, 18.0));
}

void X32Protocol::setChannelHpf(int channel, bool on, double freqHz) {
    const QString ch = x32Channel(channel);
    sendParameter(ch + "/preamp/hpon", on ? 1 : 0);
    sendParameter(ch + "/preamp/hpf", logNorm(freqHz, 20.0, 400.0));
}

void X32Protocol::setChannelEqOn(int channel, bool on) {
    sendParameter(x32Channel(channel) + "/eq/on", on ? 1 : 0);
}

void X32Protocol::setChannelEqBand(int channel, int band, bool /*on*/, int type, double freqHz,
                                   double gainDb, double q) {
    // X32 EQ has no per-band enable; the band's type selects its shape.
    const QString prefix = QString("%1/eq/%2").arg(x32Channel(channel)).arg(band);
    sendParameter(prefix + "/type", type);
    sendParameter(prefix + "/f", logNorm(freqHz, 20.0, 20000.0));
    sendParameter(prefix + "/g", linNorm(gainDb, -15.0, 15.0));
    // X32 Q is descending: norm 0.0 = Q10.0, norm 1.0 = Q0.3
    sendParameter(prefix + "/q", 1.0f - logNorm(q, 0.3, 10.0));
}

void X32Protocol::setChannelDynamics(int channel, bool on, double thresholdDb, double ratio,
                                     double attackMs, double releaseMs, double makeupDb) {
    const QString ch = x32Channel(channel);
    sendParameter(ch + "/dyn/on", on ? 1 : 0);
    sendParameter(ch + "/dyn/thr", linNorm(thresholdDb, -60.0, 0.0));
    sendParameter(ch + "/dyn/ratio", x32RatioIndex(ratio));
    sendParameter(ch + "/dyn/attack", linNorm(attackMs, 0.0, 120.0));
    sendParameter(ch + "/dyn/release", logNorm(releaseMs, 5.0, 4000.0));
    sendParameter(ch + "/dyn/mgain", linNorm(makeupDb, 0.0, 24.0));
}

void X32Protocol::setChannelName(int channel, const QString& name) {
    // scribble-strip label; the console truncates to its display width
    sendParameter(x32Channel(channel) + "/config/name", name);
}

void X32Protocol::setChannelColor(int channel, int color) {
    // /config/color is a palette index (0..15)
    sendParameter(x32Channel(channel) + "/config/color", color);
}

void X32Protocol::setDcaMute(int dca, bool muted) {
    // X32 has no /dca/N/mute; use /dca/N/on (1 = unmuted, 0 = muted)
    sendParameter(QString("/dca/%1/on").arg(dca), muted ? 0 : 1);
}

void X32Protocol::setDcaFaderDb(int dca, double dB) {
    sendParameter(QString("/dca/%1/fader").arg(dca), x32FaderFromDb(dB));
}

void X32Protocol::setDcaName(int dca, const QString& name) {
    sendParameter(QString("/dca/%1/config/name").arg(dca), name);
}

void X32Protocol::setChannelDcaMask(int channel, quint32 mask) {
    // /ch/NN/grp/dca: int bitmask, bit d-1 = member of DCA d
    sendParameter(x32Channel(channel) + "/grp/dca", static_cast<int>(mask));
}

void X32Protocol::setBusDcaMask(int bus, quint32 mask) {
    sendParameter(x32Bus(bus) + "/grp/dca", static_cast<int>(mask));
}

std::optional<quint32> X32Protocol::readChannelDcaMask(int channel) {
    const QVariant value = getParameter(x32Channel(channel) + "/grp/dca");
    if (!value.isValid())
        return std::nullopt;
    return static_cast<quint32>(value.toInt());
}

std::optional<quint32> X32Protocol::readBusDcaMask(int bus) {
    const QVariant value = getParameter(x32Bus(bus) + "/grp/dca");
    if (!value.isValid())
        return std::nullopt;
    return static_cast<quint32>(value.toInt());
}

void X32Protocol::refresh() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
    }
}

void X32Protocol::requestConsoleNames(int count) {
    if (m_connectionState != ConnectionState::Connected)
        return;
    for (int i = 1; i <= count; ++i) {
        const QString idx = QStringLiteral("%1").arg(i, 3, 10, QChar('0'));
        requestParameter(QStringLiteral("/-show/showfile/snippet/%1/name").arg(idx));
        requestParameter(QStringLiteral("/-show/showfile/scene/%1/name").arg(idx));
    }
}

void X32Protocol::onTransportConnected() {
    // transport connected, but we still wait for /xinfo response
}

void X32Protocol::onTransportDisconnected() {
    if (m_connectionState == ConnectionState::Connected) {
        emit connectionLost();
        startReconnection();
    }
}

void X32Protocol::onTransportError(const QString& error) {
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

void X32Protocol::onMessageReceived(const QString& path, const QVariant& value) {
    processResponse(path, value);
}

void X32Protocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastResponseTime > 0 && (now - m_lastResponseTime) > RESPONSE_TIMEOUT) {
            setStatus("Connection lost - no response from mixer");
            emit connectionLost();
            startReconnection();
            return;
        }

        m_transport.send("/xremote");
        m_transport.send(QString("/meters"), QString("/meters/1"));
    }
}

void X32Protocol::onConnectionTimeout() {
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

void X32Protocol::onRequestTimeoutCheck() {
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
        X32PendingRequest req = m_pendingRequests.take(path);
        if (req.callback) {
            req.callback(path, QVariant(), false);
        }
        emit requestTimeout(path);
    }
}

void X32Protocol::onReconnectAttempt() {
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

    m_waitingForXinfo = true;
    // the reference probes with /info, and some consoles answer only that; /xinfo
    // carries the richer payload, so ask for both and connect on whichever lands
    m_transport.send("/info");
    m_transport.send("/xinfo");
    m_connectionTimer.start(m_connectionTimeoutMs);
}

void X32Protocol::processResponse(const QString& path, const QVariant& value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastResponseTime = now;

    if (path == "/xinfo" || path == "/info") {
        handleXinfoResponse(value);
        return;
    }

    if (path == "/meters/1") {
        parseMeters(value.toByteArray());
        return;
    }

    // console snippet/scene name responses -> name cache
    static const QRegularExpression nameRe(
        QStringLiteral("^/-show/showfile/(scene|snippet)/(\\d+)/name$"));
    const QRegularExpressionMatch nameMatch = nameRe.match(path);
    if (nameMatch.hasMatch()) {
        emit consoleNameReceived(nameMatch.captured(1) == QLatin1String("scene"),
                                 nameMatch.captured(2).toInt(), value.toString());
    }

    // input-channel fader move -> semantic signal (used by record-faders)
    static const QRegularExpression faderRe(QStringLiteral("^/ch/(\\d+)/mix/fader$"));
    const QRegularExpressionMatch faderMatch = faderRe.match(path);
    if (faderMatch.hasMatch() && value.isValid()) {
        emit channelFaderChanged(faderMatch.captured(1).toInt(), x32DbFromFader(value.toDouble()));
    }

    if (m_pendingRequests.contains(path)) {
        X32PendingRequest req = m_pendingRequests.take(path);
        qint64 roundTrip = now - req.sentTime;
        updateLatency(roundTrip);

        if (req.callback) {
            req.callback(path, value, true);
        }
    }

    if (value.isValid()) {
        // the console reports faders as positions; publish dB so consumers do not
        // have to know which console they are looking at
        const QVariant cached =
            isFaderPath(path) ? QVariant(x32DbFromFader(value.toDouble())) : value;
        m_parameterCache[path] = cached;
        emit parameterChanged(path, cached);
    }
}

void X32Protocol::parseMeters(const QByteArray& blob) {
    // /meters/1 blob content: [uint32 LE count][count * float32 LE], each a
    // linear input-channel level 0..1. Map the first N to channels 1..N.
    if (blob.size() < 4)
        return;
    const auto* bytes = reinterpret_cast<const uchar*>(blob.constData());
    const int count = static_cast<int>(qFromLittleEndian<quint32>(bytes));
    const int maxCh = std::min(count, m_capabilities.inputChannels);
    for (int i = 0; i < maxCh; ++i) {
        const int off = 4 + i * 4;
        if (off + 4 > blob.size())
            break;
        const quint32 raw = qFromLittleEndian<quint32>(bytes + off);
        float level = 0.0f;
        std::memcpy(&level, &raw, sizeof(float));
        emit channelMeter(i + 1, level);
    }
}

void X32Protocol::handleXinfoResponse([[maybe_unused]] const QVariant& value) {
    m_connectionTimer.stop();
    m_waitingForXinfo = false;

    if (m_connectionState == ConnectionState::Connecting ||
        m_connectionState == ConnectionState::Reconnecting) {
        setConnectionState(ConnectionState::Connected);
        setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

        m_keepAliveTimer.start(KEEPALIVE_INTERVAL);
        m_requestTimeoutTimer.start();
        m_reconnectAttempts = 0;

        m_transport.send("/xremote");
        // subscribe to input-channel meters (bank 1); renewed on keep-alive
        m_transport.send(QString("/meters"), QString("/meters/1"));
        requestDcaMembership();
        emit connected();
    }
}

void X32Protocol::requestDcaMembership() {
    if (m_connectionState != ConnectionState::Connected)
        return;
    for (int i = 1; i <= m_capabilities.inputChannels && i <= MAX_X32_INPUT_CHANNELS; ++i)
        requestParameter(x32Channel(i) + "/grp/dca");
    for (int i = 1; i <= m_capabilities.mixBuses && i <= MAX_X32_MIX_BUSES; ++i)
        requestParameter(x32Bus(i) + "/grp/dca");
}

void X32Protocol::startReconnection() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_requestTimeoutTimer.stop();

    setConnectionState(ConnectionState::Reconnecting);
    m_reconnectTimer.start(m_reconnectDelayMs);
}

void X32Protocol::updateLatency(qint64 roundTripMs) {
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

void X32Protocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void X32Protocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
