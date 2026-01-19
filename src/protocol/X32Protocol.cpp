#include "X32Protocol.h"
#include "core/Cue.h"
#include <QDateTime>
#include <QNetworkDatagram>

namespace StageBlend {

X32Protocol::X32Protocol(QObject* parent) : MixerProtocol(parent) {
    // set default snapshot parameters for X32
    // channel faders & mutes for channels 1-32
    for (int i = 1; i <= 32; ++i) {
        m_snapshotParams.append(QString("/ch/%1/mix/fader").arg(i, 2, 10, QChar('0')));
        m_snapshotParams.append(QString("/ch/%1/mix/on").arg(i, 2, 10, QChar('0')));
    }
    // bus masters 1-16
    for (int i = 1; i <= 16; ++i) {
        m_snapshotParams.append(QString("/bus/%1/mix/fader").arg(i, 2, 10, QChar('0')));
        m_snapshotParams.append(QString("/bus/%1/mix/on").arg(i, 2, 10, QChar('0')));
    }
    // main LR
    m_snapshotParams.append("/main/st/mix/fader");
    m_snapshotParams.append("/main/st/mix/on");
    // dCA faders 1-8
    for (int i = 1; i <= 8; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }

    // socket for receiving responses
    QObject::connect(&m_socket, &QUdpSocket::readyRead, this, &X32Protocol::onReadyRead);

    // keep-alive timer
    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this, &X32Protocol::onKeepAliveTimeout);

    // connection timeout timer
    m_connectionTimer.setSingleShot(true);
    QObject::connect(&m_connectionTimer, &QTimer::timeout, this, &X32Protocol::onConnectionTimeout);

    // request timeout check timer (runs periodically)
    m_requestTimeoutTimer.setInterval(500); // check every 500ms
    QObject::connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
                     &X32Protocol::onRequestTimeoutCheck);

    // reconnection timer
    m_reconnectTimer.setSingleShot(true);
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this, &X32Protocol::onReconnectAttempt);
}

X32Protocol::~X32Protocol() { disconnect(); }

bool X32Protocol::connect(const QString& host, int port) {
#ifdef LIBLO_DISABLED
    Q_UNUSED(host);
    Q_UNUSED(port);
    setStatus("OSC support not available (liblo not installed)");
    emit connectionError("OSC support not available");
    return false;
#else
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port;
    m_reconnectAttempts = 0;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(port));

    // create liblo address for sending
    m_oscAddress =
        lo_address_new(host.toUtf8().constData(), QString::number(port).toUtf8().constData());
    if (!m_oscAddress) {
        setStatus("Failed to create OSC address");
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to create OSC address");
        return false;
    }

    // bind UDP socket for receiving
    if (!m_socket.bind(QHostAddress::Any, 0)) {
        setStatus("Failed to bind UDP socket");
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to bind UDP socket");
        return false;
    }

    // send /xinfo to verify connection
    m_waitingForXinfo = true;
    sendOscMessage("/xinfo");

    // start connection timeout
    m_connectionTimer.start(m_connectionTimeoutMs);

    return true;
#endif
}

void X32Protocol::disconnect() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_reconnectTimer.stop();
    m_requestTimeoutTimer.stop();

#ifndef LIBLO_DISABLED
    if (m_oscAddress) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
    }
#endif

    m_socket.close();
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
#ifdef LIBLO_DISABLED
    if (m_connectionState != ConnectionState::Connected)
        return;
#else
    if (m_connectionState != ConnectionState::Connected || !m_oscAddress)
        return;
#endif

    switch (value.typeId()) {
    case QMetaType::Float:
    case QMetaType::Double:
        sendOscMessage(path, value.toFloat());
        break;
    case QMetaType::Int:
    case QMetaType::Bool:
        sendOscMessage(path, value.toInt());
        break;
    case QMetaType::QString:
        sendOscMessage(path, value.toString());
        break;
    default:
        // try float as default
        sendOscMessage(path, value.toFloat());
        break;
    }

    // update cache
    m_parameterCache[path] = value;
}

QVariant X32Protocol::getParameter(const QString& path) { return m_parameterCache.value(path); }

void X32Protocol::requestParameter(const QString& path) {
#ifdef LIBLO_DISABLED
    if (m_connectionState != ConnectionState::Connected)
        return;
#else
    if (m_connectionState != ConnectionState::Connected || !m_oscAddress)
        return;
#endif

    // track request for timeout
    PendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = nullptr;
    m_pendingRequests[path] = req;

    sendOscMessage(path);
}

void X32Protocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
#ifdef LIBLO_DISABLED
    bool canSend = (m_connectionState == ConnectionState::Connected);
#else
    bool canSend = (m_connectionState == ConnectionState::Connected && m_oscAddress);
#endif
    if (!canSend) {
        if (callback) {
            callback(path, QVariant(), false);
        }
        return;
    }

    // track request with callback
    PendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = callback;
    m_pendingRequests[path] = req;

    sendOscMessage(path);
}

void X32Protocol::captureSnapshot(Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // request all snapshot parameters
    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
    }

    // use cached values (async responses will update cache)
    QJsonObject params;
    for (const QString& param : m_snapshotParams) {
        if (m_parameterCache.contains(param)) {
            params[param] = QJsonValue::fromVariant(m_parameterCache[param]);
        }
    }
    cue.setParameters(params);
    emit snapshotCaptured();
}

void X32Protocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

QJsonObject X32Protocol::captureCurrentState() {
    QJsonObject state;
    for (const QString& param : m_snapshotParams) {
        if (m_parameterCache.contains(param)) {
            state[param] = QJsonValue::fromVariant(m_parameterCache[param]);
        }
    }
    return state;
}

void X32Protocol::refresh() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // request all parameters
    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
    }
}

void X32Protocol::onReadyRead() {
    while (m_socket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket.receiveDatagram();
        parseOscMessage(datagram.data());
    }
}

void X32Protocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        // check if we've received any response recently
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastResponseTime > 0 && (now - m_lastResponseTime) > (KEEPALIVE_INTERVAL * 3)) {
            // no response for 3 keep-alive intervals, connection likely lost
            setStatus("Connection lost - no response from mixer");
            emit connectionLost();
            startReconnection();
            return;
        }

        // send /xremote to keep subscription active
        sendOscMessage("/xremote");
    }
}

void X32Protocol::onConnectionTimeout() {
    if (m_connectionState == ConnectionState::Connecting) {
        setStatus("Connection timeout");
        emit connectionError("Connection timeout - no response from mixer");

        // try reconnecting
        startReconnection();
    }
}

void X32Protocol::onRequestTimeoutCheck() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QDateTime now = QDateTime::currentDateTime();
    QStringList timedOut;

    // check for timed out requests
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (it->timestamp.msecsTo(now) > m_requestTimeoutMs) {
            timedOut.append(it.key());
        }
    }

    // process timeouts
    for (const QString& path : timedOut) {
        PendingRequest req = m_pendingRequests.take(path);

        // invoke callback with failure
        if (req.callback) {
            req.callback(path, QVariant(), false);
        }

        emit requestTimeout(path);
    }
}

void X32Protocol::onReconnectAttempt() {
#ifdef LIBLO_DISABLED
    setConnectionState(ConnectionState::Disconnected);
    return;
#else
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

    // cleanup existing connection
    if (m_oscAddress) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
    }
    m_socket.close();

    // create new address
    m_oscAddress =
        lo_address_new(m_host.toUtf8().constData(), QString::number(m_port).toUtf8().constData());
    if (!m_oscAddress) {
        // schedule next attempt with exponential backoff
        int delay = m_reconnectDelayMs * (1 << (m_reconnectAttempts - 1));
        m_reconnectTimer.start(delay);
        return;
    }

    // rebind socket
    if (!m_socket.bind(QHostAddress::Any, 0)) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
        int delay = m_reconnectDelayMs * (1 << (m_reconnectAttempts - 1));
        m_reconnectTimer.start(delay);
        return;
    }

    // send /xinfo to verify connection
    m_waitingForXinfo = true;
    sendOscMessage("/xinfo");
    m_connectionTimer.start(m_connectionTimeoutMs);
#endif
}

void X32Protocol::sendOscMessage(const QString& path) {
#ifndef LIBLO_DISABLED
    if (!m_oscAddress)
        return;
    lo_send(m_oscAddress, path.toUtf8().constData(), nullptr);
#else
    Q_UNUSED(path);
#endif
}

void X32Protocol::sendOscMessage(const QString& path, float value) {
#ifndef LIBLO_DISABLED
    if (!m_oscAddress)
        return;
    lo_send(m_oscAddress, path.toUtf8().constData(), "f", value);
#else
    Q_UNUSED(path);
    Q_UNUSED(value);
#endif
}

void X32Protocol::sendOscMessage(const QString& path, int value) {
#ifndef LIBLO_DISABLED
    if (!m_oscAddress)
        return;
    lo_send(m_oscAddress, path.toUtf8().constData(), "i", value);
#else
    Q_UNUSED(path);
    Q_UNUSED(value);
#endif
}

void X32Protocol::sendOscMessage(const QString& path, const QString& value) {
#ifndef LIBLO_DISABLED
    if (!m_oscAddress)
        return;
    lo_send(m_oscAddress, path.toUtf8().constData(), "s", value.toUtf8().constData());
#else
    Q_UNUSED(path);
    Q_UNUSED(value);
#endif
}

void X32Protocol::parseOscMessage(const QByteArray& data) {
    // basic OSC message parsing
    // OSC format: path (null-padded to 4-byte boundary), type tag string, arguments

    if (data.size() < 4)
        return;

    // extract path
    int pathEnd = data.indexOf('\0');
    if (pathEnd < 0)
        return;
    QString path = QString::fromUtf8(data.left(pathEnd));

    // skip to type tag (4-byte aligned)
    int typeStart = ((pathEnd + 4) / 4) * 4;
    if (typeStart >= data.size()) {
        // message with no arguments (like /xinfo response)
        processResponse(path, QVariant());
        return;
    }

    // find type tag
    if (data.at(typeStart) != ',')
        return;
    int typeEnd = data.indexOf('\0', typeStart);
    if (typeEnd < 0)
        return;
    QString types = QString::fromUtf8(data.mid(typeStart + 1, typeEnd - typeStart - 1));

    // skip to arguments (4-byte aligned)
    int argStart = ((typeEnd + 4) / 4) * 4;
    if (argStart > data.size())
        return;

    // parse first argument based on type
    if (!types.isEmpty()) {
        char type = types.at(0).toLatin1();
        QVariant value;

        switch (type) {
        case 'f': {
            if (argStart + 4 <= data.size()) {
                // big-endian float
                quint32 raw = (static_cast<quint8>(data[argStart]) << 24) |
                              (static_cast<quint8>(data[argStart + 1]) << 16) |
                              (static_cast<quint8>(data[argStart + 2]) << 8) |
                              static_cast<quint8>(data[argStart + 3]);
                float f;
                memcpy(&f, &raw, sizeof(float));
                value = f;
            }
            break;
        }
        case 'i': {
            if (argStart + 4 <= data.size()) {
                qint32 i = (static_cast<quint8>(data[argStart]) << 24) |
                           (static_cast<quint8>(data[argStart + 1]) << 16) |
                           (static_cast<quint8>(data[argStart + 2]) << 8) |
                           static_cast<quint8>(data[argStart + 3]);
                value = i;
            }
            break;
        }
        case 's': {
            int strEnd = data.indexOf('\0', argStart);
            if (strEnd > argStart) {
                value = QString::fromUtf8(data.mid(argStart, strEnd - argStart));
            }
            break;
        }
        }

        processResponse(path, value);
    }
}

void X32Protocol::processResponse(const QString& path, const QVariant& value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastResponseTime = now;

    // handle /xinfo response for connection validation
    if (path == "/xinfo") {
        handleXinfoResponse(value);
        return;
    }

    // check if this was a pending request
    if (m_pendingRequests.contains(path)) {
        PendingRequest req = m_pendingRequests.take(path);

        // calculate latency
        qint64 roundTrip = now - req.sentTime;
        updateLatency(roundTrip);

        // invoke callback
        if (req.callback) {
            req.callback(path, value, true);
        }
    }

    // update cache & emit signal
    if (value.isValid()) {
        m_parameterCache[path] = value;
        emit parameterChanged(path, value);
    }
}

void X32Protocol::handleXinfoResponse(const QVariant& value) {
    m_connectionTimer.stop();
    m_waitingForXinfo = false;

    if (m_connectionState == ConnectionState::Connecting ||
        m_connectionState == ConnectionState::Reconnecting) {
        // connection successful
        setConnectionState(ConnectionState::Connected);
        setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

        // start keep-alive timer
        m_keepAliveTimer.start(KEEPALIVE_INTERVAL);

        // start request timeout checker
        m_requestTimeoutTimer.start();

        // reset reconnect counter
        m_reconnectAttempts = 0;

        // subscribe to meter updates
        sendOscMessage("/xremote");

        emit connected();
    }
}

void X32Protocol::startReconnection() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_requestTimeoutTimer.stop();

    setConnectionState(ConnectionState::Reconnecting);

    // start first reconnection attempt
    m_reconnectTimer.start(m_reconnectDelayMs);
}

void X32Protocol::updateLatency(qint64 roundTripMs) {
    m_latencyHistory.append(static_cast<int>(roundTripMs));

    // keep history size limited
    while (m_latencyHistory.size() > LATENCY_HISTORY_SIZE) {
        m_latencyHistory.removeFirst();
    }

    // calculate average latency
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

// factory function implementation
MixerProtocol* createMixerProtocol(const QString& type, QObject* parent) {
    if (type.toLower() == "x32" || type.toLower() == "m32") {
        return new X32Protocol(parent);
    }
    // TODO: add yamaha, allen-heath, etc.
    return nullptr;
}

} // namespace StageBlend
