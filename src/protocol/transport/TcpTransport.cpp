#include "TcpTransport.h"

namespace OpenMix {

TcpTransport::TcpTransport(QObject* parent) : QObject(parent) {
    QObject::connect(&m_socket, &QTcpSocket::connected, this, &TcpTransport::onConnected);
    QObject::connect(&m_socket, &QTcpSocket::disconnected, this, &TcpTransport::onDisconnected);
    QObject::connect(&m_socket, &QTcpSocket::errorOccurred, this, &TcpTransport::onError);
    QObject::connect(&m_socket, &QTcpSocket::readyRead, this, &TcpTransport::onReadyRead);

    m_connectionTimer.setSingleShot(true);
    QObject::connect(&m_connectionTimer, &QTimer::timeout, this,
                     &TcpTransport::onConnectionTimeout);

    m_reconnectTimer.setSingleShot(true);
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this, &TcpTransport::onReconnectAttempt);
}

TcpTransport::~TcpTransport() { disconnect(); }

bool TcpTransport::connect(const QString& host, int port) {
    if (isConnected()) {
        disconnect();
    }

    m_host = host;
    m_port = port;
    m_reconnectAttempts = 0;
    m_wasConnected = false;

    m_socket.connectToHost(host, port);
    m_connectionTimer.start(m_connectionTimeoutMs);

    return true; // async connection, check signals for result
}

void TcpTransport::disconnect() {
    m_connectionTimer.stop();
    m_reconnectTimer.stop();
    m_reconnectAttempts = 0;

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState) {
            m_socket.waitForDisconnected(1000);
        }
    }
}

bool TcpTransport::isConnected() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool TcpTransport::send(const QByteArray& data) {
    if (!isConnected())
        return false;

    qint64 written = m_socket.write(data);
    return written == data.size();
}

void TcpTransport::onConnected() {
    m_connectionTimer.stop();
    m_reconnectAttempts = 0;
    m_wasConnected = true;
    emit connected();
}

void TcpTransport::onDisconnected() {
    m_connectionTimer.stop();

    if (m_wasConnected && m_reconnectEnabled) {
        emit connectionLost();
        startReconnection();
    } else {
        emit disconnected();
    }
}

void TcpTransport::onError(QAbstractSocket::SocketError error) {
    m_connectionTimer.stop();

    QString errorMsg;
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = "Connection refused";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = "Host not found";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMsg = "Connection timeout";
        break;
    case QAbstractSocket::NetworkError:
        errorMsg = "Network error";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorMsg = "Remote host closed connection";
        if (m_wasConnected && m_reconnectEnabled) {
            emit connectionLost();
            startReconnection();
            return;
        }
        break;
    default:
        errorMsg = m_socket.errorString();
        break;
    }

    emit connectionError(errorMsg);
}

void TcpTransport::onReadyRead() {
    QByteArray data = m_socket.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpTransport::onConnectionTimeout() {
    if (m_socket.state() == QAbstractSocket::ConnectingState) {
        m_socket.abort();
        emit connectionError("Connection timeout");

        if (m_reconnectEnabled) {
            startReconnection();
        }
    }
}

void TcpTransport::onReconnectAttempt() {
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        emit connectionError("Failed to reconnect after maximum attempts");
        emit disconnected();
        return;
    }

    m_reconnectAttempts++;
    emit reconnecting(m_reconnectAttempts, m_maxReconnectAttempts);

    m_socket.connectToHost(m_host, m_port);
    m_connectionTimer.start(m_connectionTimeoutMs);
}

void TcpTransport::startReconnection() {
    if (!m_reconnectEnabled)
        return;

    // exponential backoff
    int delay = m_reconnectDelayMs * (1 << m_reconnectAttempts);
    m_reconnectTimer.start(delay);
}

} // namespace OpenMix
