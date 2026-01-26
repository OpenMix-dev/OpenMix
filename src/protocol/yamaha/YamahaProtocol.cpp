#include "YamahaProtocol.h"
#include "../../core/Cue.h"
#include <QNetworkDatagram>
#include <cstring>
#include <lo/lo.h>

namespace OpenMix {

YamahaProtocol::YamahaProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps) {

    m_receivePort = caps.defaultPort; // 8000 for Yamaha OSC receive
    m_transmitPort = 9000;            // Yamaha OSC transmit port

    initializeSnapshotParams();

    QObject::connect(&m_receiveSocket, &QUdpSocket::readyRead, this, &YamahaProtocol::onReadyRead);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &YamahaProtocol::onKeepAliveTimeout);

    m_connectionTimer.setSingleShot(true);
    QObject::connect(&m_connectionTimer, &QTimer::timeout, this,
                     &YamahaProtocol::onConnectionTimeout);

    m_requestTimeoutTimer.setInterval(500);
    QObject::connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
                     &YamahaProtocol::onRequestTimeoutCheck);

    m_reconnectTimer.setSingleShot(true);
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this,
                     &YamahaProtocol::onReconnectAttempt);
}

YamahaProtocol::~YamahaProtocol() { disconnect(); }

void YamahaProtocol::initializeSnapshotParams() { rebuildSnapshotParams(); }

void YamahaProtocol::rebuildSnapshotParams() {
    m_snapshotParams.clear();

    // input channel fader/on parameters
    for (int i = 1; i <= m_capabilities.inputChannels; ++i) {
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

        // effect send parameters
        if (m_capabilities.supportsEffectSends) {
            int sends = qMin(m_capabilities.effectSendBuses, 16);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_snapshotParams.append(sendPrefix + "/level");
                m_snapshotParams.append(sendPrefix + "/on");
            }
        }
    }

    // DCA parameters, Yamaha uses /dca/X/fader and /dca/X/on (NOT /mute)
    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

bool YamahaProtocol::connect(const QString& host, int port) {
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port > 0 ? port : m_receivePort;
    m_reconnectAttempts = 0;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(m_transmitPort));

    // create OSC address for sending to transmit port
    m_oscAddress = lo_address_new(host.toUtf8().constData(),
                                  QString::number(m_transmitPort).toUtf8().constData());
    if (!m_oscAddress) {
        setStatus("Failed to create OSC address");
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to create OSC address");
        return false;
    }

    // bind receive socket to receive port
    if (!m_receiveSocket.bind(QHostAddress::Any, m_port)) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
        setStatus("Failed to bind UDP receive socket");
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to bind UDP socket on port " + QString::number(m_port));
        return false;
    }

    // send model query to verify connection
    m_waitingForModel = true;
    sendOscMessage("/sys/model");
    m_connectionTimer.start(m_connectionTimeoutMs);

    return true;
}

void YamahaProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_reconnectTimer.stop();
    m_requestTimeoutTimer.stop();

    if (m_oscAddress) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
    }

    m_receiveSocket.close();

    m_parameterCache.clear();
    m_pendingRequests.clear();
    m_latencyHistory.clear();
    m_latencyMs = 0;
    m_reconnectAttempts = 0;
    m_waitingForModel = false;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void YamahaProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

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
        sendOscMessage(path, value.toFloat());
        break;
    }

    m_parameterCache[path] = value;
}

QVariant YamahaProtocol::getParameter(const QString& path) { return m_parameterCache.value(path); }

void YamahaProtocol::requestParameter(const QString& path) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    YamahaPendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = nullptr;
    m_pendingRequests[path] = req;

    sendOscMessage(path);
}

void YamahaProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_connectionState != ConnectionState::Connected) {
        if (callback) {
            callback(path, QVariant(), false);
        }
        return;
    }

    YamahaPendingRequest req;
    req.path = path;
    req.timestamp = QDateTime::currentDateTime();
    req.sentTime = QDateTime::currentMSecsSinceEpoch();
    req.callback = callback;
    m_pendingRequests[path] = req;

    sendOscMessage(path);
}

void YamahaProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void YamahaProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    // Yamaha scene recall: /scene/recall with scene number
    sendOscMessage("/scene/recall", sceneNumber);
}

void YamahaProtocol::refresh() {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
    }
}

void YamahaProtocol::onReadyRead() {
    while (m_receiveSocket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_receiveSocket.receiveDatagram();
        parseOscMessage(datagram.data());
    }
}

void YamahaProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastResponseTime > 0 && (now - m_lastResponseTime) > (KEEPALIVE_INTERVAL * 3)) {
            setStatus("Connection lost - no response from mixer");
            emit connectionLost();
            startReconnection();
            return;
        }

        // query model as keep-alive
        sendOscMessage("/sys/model");
    }
}

void YamahaProtocol::onConnectionTimeout() {
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
            int delay = m_reconnectDelayMs * (1 << m_reconnectAttempts);
            m_reconnectTimer.start(delay);
        }
    }
}

void YamahaProtocol::onRequestTimeoutCheck() {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QStringList timedOut;

    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (it->timestamp.msecsTo(now) > m_requestTimeoutMs) {
            timedOut.append(it.key());
        }
    }

    for (const QString& path : timedOut) {
        YamahaPendingRequest req = m_pendingRequests.take(path);
        if (req.callback) {
            req.callback(path, QVariant(), false);
        }
        emit requestTimeout(path);
    }
}

void YamahaProtocol::onReconnectAttempt() {
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

    // clean up old connection
    if (m_oscAddress) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
    }
    m_receiveSocket.close();

    // recreate connection
    m_oscAddress = lo_address_new(m_host.toUtf8().constData(),
                                  QString::number(m_transmitPort).toUtf8().constData());
    if (!m_oscAddress) {
        int delay = m_reconnectDelayMs * (1 << (m_reconnectAttempts - 1));
        m_reconnectTimer.start(delay);
        return;
    }

    if (!m_receiveSocket.bind(QHostAddress::Any, m_port)) {
        lo_address_free(m_oscAddress);
        m_oscAddress = nullptr;
        int delay = m_reconnectDelayMs * (1 << (m_reconnectAttempts - 1));
        m_reconnectTimer.start(delay);
        return;
    }

    m_waitingForModel = true;
    sendOscMessage("/sys/model");
    m_connectionTimer.start(m_connectionTimeoutMs);
}

void YamahaProtocol::sendOscMessage(const QString& path) {
    if (!m_oscAddress) {
        return;
    }
    lo_send(m_oscAddress, path.toUtf8().constData(), nullptr);
}

void YamahaProtocol::sendOscMessage(const QString& path, float value) {
    if (!m_oscAddress) {
        return;
    }
    lo_send(m_oscAddress, path.toUtf8().constData(), "f", value);
}

void YamahaProtocol::sendOscMessage(const QString& path, int value) {
    if (!m_oscAddress) {
        return;
    }
    lo_send(m_oscAddress, path.toUtf8().constData(), "i", value);
}

void YamahaProtocol::sendOscMessage(const QString& path, const QString& value) {
    if (!m_oscAddress) {
        return;
    }
    lo_send(m_oscAddress, path.toUtf8().constData(), "s", value.toUtf8().constData());
}

void YamahaProtocol::parseOscMessage(const QByteArray& data) {
    if (data.size() < 4) {
        return;
    }

    // extract path
    int pathEnd = data.indexOf('\0');
    if (pathEnd < 0) {
        return;
    }
    QString path = QString::fromUtf8(data.left(pathEnd));

    // skip to type tag (4-byte aligned)
    int typeStart = ((pathEnd + 4) / 4) * 4;
    if (typeStart >= data.size()) {
        processResponse(path, QVariant());
        return;
    }

    // find type tag
    if (data.at(typeStart) != ',') {
        return;
    }
    int typeEnd = data.indexOf('\0', typeStart);
    if (typeEnd < 0) {
        return;
    }
    QString types = QString::fromUtf8(data.mid(typeStart + 1, typeEnd - typeStart - 1));

    // skip to arguments (4-byte aligned)
    int argOffset = ((typeEnd + 4) / 4) * 4;
    if (argOffset > data.size()) {
        return;
    }

    if (!types.isEmpty()) {
        QVariant value = parseOscArgument(data, argOffset, types.at(0).toLatin1());
        processResponse(path, value);
    }
}

QVariant YamahaProtocol::parseOscArgument(const QByteArray& data, int& offset, char type) {
    switch (type) {
    case 'f': {
        if (offset + 4 <= data.size()) {
            quint32 raw = (static_cast<quint8>(data[offset]) << 24) |
                          (static_cast<quint8>(data[offset + 1]) << 16) |
                          (static_cast<quint8>(data[offset + 2]) << 8) |
                          static_cast<quint8>(data[offset + 3]);
            float f;
            std::memcpy(&f, &raw, sizeof(float));
            offset += 4;
            return f;
        }
        break;
    }
    case 'i': {
        if (offset + 4 <= data.size()) {
            qint32 i = (static_cast<quint8>(data[offset]) << 24) |
                       (static_cast<quint8>(data[offset + 1]) << 16) |
                       (static_cast<quint8>(data[offset + 2]) << 8) |
                       static_cast<quint8>(data[offset + 3]);
            offset += 4;
            return i;
        }
        break;
    }
    case 's': {
        int strEnd = data.indexOf('\0', offset);
        if (strEnd > offset) {
            QString str = QString::fromUtf8(data.mid(offset, strEnd - offset));
            offset = ((strEnd + 4) / 4) * 4;
            return str;
        }
        break;
    }
    case 'b': {
        if (offset + 4 <= data.size()) {
            qint32 size = (static_cast<quint8>(data[offset]) << 24) |
                          (static_cast<quint8>(data[offset + 1]) << 16) |
                          (static_cast<quint8>(data[offset + 2]) << 8) |
                          static_cast<quint8>(data[offset + 3]);
            offset += 4;
            if (offset + size <= data.size()) {
                QByteArray blob = data.mid(offset, size);
                offset += ((size + 3) / 4) * 4;
                return blob;
            }
        }
        break;
    }
    }

    return QVariant();
}

void YamahaProtocol::processResponse(const QString& path, const QVariant& value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastResponseTime = now;

    if (path == "/sys/model") {
        handleModelResponse(value);
        return;
    }

    if (m_pendingRequests.contains(path)) {
        YamahaPendingRequest req = m_pendingRequests.take(path);
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

void YamahaProtocol::handleModelResponse(const QVariant& value) {
    Q_UNUSED(value);
    m_connectionTimer.stop();
    m_waitingForModel = false;

    if (m_connectionState == ConnectionState::Connecting ||
        m_connectionState == ConnectionState::Reconnecting) {
        setConnectionState(ConnectionState::Connected);
        setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_transmitPort));

        m_keepAliveTimer.start(KEEPALIVE_INTERVAL);
        m_requestTimeoutTimer.start();
        m_reconnectAttempts = 0;

        emit connected();
    }
}

void YamahaProtocol::startReconnection() {
    m_keepAliveTimer.stop();
    m_connectionTimer.stop();
    m_requestTimeoutTimer.stop();

    setConnectionState(ConnectionState::Reconnecting);
    m_reconnectTimer.start(m_reconnectDelayMs);
}

void YamahaProtocol::updateLatency(qint64 roundTripMs) {
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
