#include "X32Protocol.h"
#include "../../core/Cue.h"
#include <QDateTime>
#include <algorithm>

namespace OpenMix {

namespace {
constexpr int MAX_X32_INPUT_CHANNELS = 32;
constexpr int MAX_X32_EFFECT_SENDS = 16;
constexpr int MAX_X32_MIX_BUSES = 16;
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
    m_parameterCache[path] = value;
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

void X32Protocol::refresh() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
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
        if (m_lastResponseTime > 0 && (now - m_lastResponseTime) > (KEEPALIVE_INTERVAL * 3)) {
            setStatus("Connection lost - no response from mixer");
            emit connectionLost();
            startReconnection();
            return;
        }

        m_transport.send("/xremote");
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
    m_transport.send("/xinfo");
    m_connectionTimer.start(m_connectionTimeoutMs);
}

void X32Protocol::processResponse(const QString& path, const QVariant& value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastResponseTime = now;

    if (path == "/xinfo") {
        handleXinfoResponse(value);
        return;
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
        m_parameterCache[path] = value;
        emit parameterChanged(path, value);
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
        emit connected();
    }
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
