#include "YamahaTcpProtocol.h"
#include "../../core/Cue.h"

namespace OpenMix {

YamahaTcpProtocol::YamahaTcpProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort; // 49280

    QObject::connect(&m_transport, &TcpTransport::connected, this,
                     &YamahaTcpProtocol::onTransportConnected);
    QObject::connect(&m_transport, &TcpTransport::disconnected, this,
                     &YamahaTcpProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &TcpTransport::connectionError, this,
                     &YamahaTcpProtocol::onTransportError);
    QObject::connect(&m_transport, &TcpTransport::connectionLost, this,
                     &YamahaTcpProtocol::onTransportConnectionLost);
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     &YamahaTcpProtocol::onDataReceived);
    QObject::connect(&m_transport, &TcpTransport::reconnecting, this,
                     &YamahaTcpProtocol::onReconnecting);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &YamahaTcpProtocol::onKeepAliveTimeout);

    QObject::connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
                     &YamahaTcpProtocol::onRequestTimeoutCheck);
}

YamahaTcpProtocol::~YamahaTcpProtocol() { disconnect(); }

bool YamahaTcpProtocol::connect(const QString& host, int port) {
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

void YamahaTcpProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_requestTimeoutTimer.stop();
    m_transport.disconnect();

    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (it->callback) {
            it->callback(it->path, QVariant(), false);
        }
    }
    m_pendingRequests.clear();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void YamahaTcpProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    if (path.startsWith("/dca/")) {
        QStringList parts = path.split('/');
        if (parts.size() >= 4) {
            int dca = parts[2].toInt();
            QString param = parts[3];

            if (param == "fader") {
                sendCommand(buildDCAFaderCommand(dca, value.toFloat()));
            } else if (param == "mute") {
                sendCommand(buildDCAMuteCommand(dca, value.toBool()));
            }
        }
    }

    m_parameterCache[path] = value;
}

QVariant YamahaTcpProtocol::getParameter(const QString& path) {
    return m_parameterCache.value(path);
}

void YamahaTcpProtocol::requestParameter(const QString& path) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // Yamaha get command format varies by model
    // generic: get MIXER:Current/DCA/Fader/Level X
    if (path.startsWith("/dca/")) {
        QStringList parts = path.split('/');
        if (parts.size() >= 4) {
            int dca = parts[2].toInt();
            QString param = parts[3];

            if (param == "fader") {
                sendCommand(QString("get MIXER:Current/DCA/Fader/Level %1 0").arg(dca - 1));
            } else if (param == "mute") {
                sendCommand(QString("get MIXER:Current/DCA/Mute/On %1 0").arg(dca - 1));
            }
        }
    }
}

void YamahaTcpProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_parameterCache.contains(path)) {
        if (callback) {
            callback(path, m_parameterCache[path], true);
        }
        return;
    }

    if (m_connectionState != ConnectionState::Connected) {
        if (callback) {
            callback(path, QVariant(), false);
        }
        return;
    }

    PendingRequest req;
    req.path = path;
    req.callback = callback;
    req.timestamp = QDateTime::currentMSecsSinceEpoch();
    m_pendingRequests[path] = req;

    if (!m_requestTimeoutTimer.isActive()) {
        m_requestTimeoutTimer.start(REQUEST_TIMEOUT_CHECK_INTERVAL);
    }

    requestParameter(path);
}

void YamahaTcpProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void YamahaTcpProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    sendCommand(buildSceneRecallCommand(sceneNumber));
}

void YamahaTcpProtocol::refresh() {
    if (m_connectionState != ConnectionState::Connected)
        return;

    for (const QString& param : m_snapshotParams) {
        requestParameter(param);
    }
}

QString YamahaTcpProtocol::buildDCAFaderCommand(int dca, float level) {
    // Yamaha DCA fader command format (CL/QL style)
    // set MIXER:Current/DCA/Fader/Level X 0 Y
    // X = DCA index (0-based), Y = level (0-1000 typically)
    int levelInt = qBound(0, static_cast<int>(level * 1000.0f), 1000);
    return QString("set MIXER:Current/DCA/Fader/Level %1 0 %2").arg(dca - 1).arg(levelInt);
}

QString YamahaTcpProtocol::buildDCAMuteCommand(int dca, bool muted) {
    // Yamaha DCA mute command format
    // set MIXER:Current/DCA/Mute/On X 0 Y
    // X = DCA index (0-based), Y = 0 or 1
    return QString("set MIXER:Current/DCA/Mute/On %1 0 %2").arg(dca - 1).arg(muted ? 1 : 0);
}

QString YamahaTcpProtocol::buildSceneRecallCommand(int sceneNumber) {
    // Yamaha scene recall command
    // ssrecall_ex MIXER:Current/Scene/Ss/Data X
    return QString("ssrecall_ex MIXER:Current/Scene/Ss/Data %1").arg(sceneNumber);
}

QString YamahaTcpProtocol::buildKeepAliveCommand() {
    // simple status query as keep-alive
    return "devinfo devicename";
}

void YamahaTcpProtocol::sendCommand(const QString& command) {
    if (!m_transport.isConnected())
        return;

    // Yamaha commands are terminated with newline
    QByteArray data = command.toUtf8() + "\n";
    m_transport.send(data);
}

void YamahaTcpProtocol::parseResponse(const QString& response) {
    // Yamaha response: OK set/get MIXER:Current/DCA/Fader/Level X 0 Y
    QStringList parts = response.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 4)
        return;

    if (parts[0] != "OK")
        return;

    QString cmd = parts[1];
    QString paramPath = parts[2];
    QString path;
    QVariant value;

    if (paramPath.contains("DCA/Fader/Level") && parts.size() >= 6) {
        int dcaIndex = parts[3].toInt();
        int rawValue = parts[5].toInt();

        path = QString("/dca/%1/fader").arg(dcaIndex + 1);
        float level = rawValue / 1000.0f;
        value = level;
        m_parameterCache[path] = level;
        emit parameterChanged(path, level);
    } else if (paramPath.contains("DCA/Mute/On") && parts.size() >= 6) {
        int dcaIndex = parts[3].toInt();
        bool muted = parts[5].toInt() != 0;

        path = QString("/dca/%1/mute").arg(dcaIndex + 1);
        value = muted;
        m_parameterCache[path] = muted;
        emit parameterChanged(path, muted);
    }

    if (!path.isEmpty() && m_pendingRequests.contains(path)) {
        PendingRequest req = m_pendingRequests.take(path);
        if (req.callback) {
            req.callback(path, value, true);
        }
    }
}

void YamahaTcpProtocol::onTransportConnected() {
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

    m_keepAliveTimer.start(KEEPALIVE_INTERVAL);

    emit connected();
}

void YamahaTcpProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void YamahaTcpProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);
}

void YamahaTcpProtocol::onTransportConnectionLost() {
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void YamahaTcpProtocol::onDataReceived(const QByteArray& data) {
    m_receiveBuffer.append(data);

    while (true) {
        int newlinePos = m_receiveBuffer.indexOf('\n');
        if (newlinePos < 0)
            break;

        QByteArray lineData = m_receiveBuffer.left(newlinePos);
        m_receiveBuffer.remove(0, newlinePos + 1);

        if (lineData.endsWith('\r')) {
            lineData.chop(1);
        }

        QString line = QString::fromUtf8(lineData);
        processLine(line);
    }
}

void YamahaTcpProtocol::processLine(const QString& line) {
    if (line.isEmpty())
        return;

    parseResponse(line);
}

void YamahaTcpProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        sendCommand(buildKeepAliveCommand());
    }
}

void YamahaTcpProtocol::onReconnecting(int attempt, int maxAttempts) {
    setStatus(QString("Reconnecting (attempt %1/%2)...").arg(attempt).arg(maxAttempts));
}

void YamahaTcpProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void YamahaTcpProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

void YamahaTcpProtocol::onRequestTimeoutCheck() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList timedOutPaths;
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        if (now - it->timestamp > REQUEST_TIMEOUT_MS) {
            timedOutPaths.append(it.key());
        }
    }

    for (const QString& path : timedOutPaths) {
        PendingRequest req = m_pendingRequests.take(path);
        if (req.callback) {
            req.callback(path, QVariant(), false);
        }
    }

    if (m_pendingRequests.isEmpty()) {
        m_requestTimeoutTimer.stop();
    }
}

} // namespace OpenMix
