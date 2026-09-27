#include "TcpTransport.h"
#include <algorithm>

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
    // drop whatever the previous session left behind, silently
    tearDown();

    m_closing = false;
    m_host = host;
    m_port = port;
    m_reconnectAttempts = 0;
    m_wasConnected = false;

    m_socket.connectToHost(host, static_cast<quint16>(port));
    m_connectionTimer.start(m_connectionTimeoutMs);

    return true; // async connection, check signals for result
}

void TcpTransport::disconnect() { tearDown(); }

void TcpTransport::tearDown() {
    m_closing = true;
    m_connectionTimer.stop();
    m_reconnectTimer.stop();
    m_reconnectAttempts = 0;
    m_wasConnected = false;

    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    // push out anything already queued (a console's goodbye frame, typically)
    // without blocking, then close; anything that could not go is dropped
    // rather than waited for
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        m_socket.flush();
        m_socket.disconnectFromHost();
    }
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
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
    if (m_closing)
        return;
    m_connectionTimer.stop();
    m_reconnectAttempts = 0;
    m_wasConnected = true;
    emit connected();
}

void TcpTransport::onDisconnected() {
    if (m_closing)
        return;
    m_connectionTimer.stop();

    if (m_wasConnected && m_reconnectEnabled) {
        emit connectionLost();
        scheduleReconnect();
    } else {
        emit disconnected();
    }
}

void TcpTransport::onError(QAbstractSocket::SocketError error) {
    if (m_closing)
        return;
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
        // the socket follows this with disconnected(), which decides whether
        // the link is retried; reporting it here as well would double up
        return;
    default:
        // Qt's string for an unmapped errno is a bare "Unknown error"; carry the
        // enum so the log can still identify the failure
        errorMsg = QString("%1 (socket error %2)").arg(m_socket.errorString()).arg(error);
        break;
    }

    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        // a non-fatal error on a live link: report it and carry on
        emit connectionError(errorMsg);
        return;
    }

    failAttempt(errorMsg);
}

void TcpTransport::onReadyRead() {
    QByteArray data = m_socket.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpTransport::onConnectionTimeout() {
    if (m_closing)
        return;
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        return;
    }

    // abort() emits nothing for a socket that never connected, but guard the
    // slots anyway so a late signal cannot count as a second failure
    m_closing = true;
    m_socket.abort();
    m_closing = false;

    failAttempt("Connection timeout");
}

void TcpTransport::failAttempt(const QString& error) {
    emit connectionError(error);

    if (m_wasConnected && m_reconnectEnabled && m_reconnectAttempts < m_maxReconnectAttempts) {
        scheduleReconnect();
        return;
    }

    if (m_wasConnected) {
        emit connectionError("Failed to reconnect after maximum attempts");
    }
    m_wasConnected = false;
    m_reconnectAttempts = 0;
    emit disconnected();
}

void TcpTransport::onReconnectAttempt() {
    if (m_closing)
        return;

    m_reconnectAttempts++;
    emit reconnecting(m_reconnectAttempts, m_maxReconnectAttempts);

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_closing = true;
        m_socket.abort();
        m_closing = false;
    }

    m_socket.connectToHost(m_host, static_cast<quint16>(m_port));
    m_connectionTimer.start(m_connectionTimeoutMs);
}

void TcpTransport::scheduleReconnect() {
    // exponential backoff from the base delay: 1x, 2x, 4x, ... capped at 30 s
    const int shift = std::min(m_reconnectAttempts, 10);
    const int delay = std::min(m_reconnectDelayMs * (1 << shift), 30000);
    m_reconnectTimer.start(delay);
}

} // namespace OpenMix
