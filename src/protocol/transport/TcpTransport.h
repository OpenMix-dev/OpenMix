#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace OpenMix {

// TCP link with a small, explicit life cycle:
//   * connect() opens one attempt; a failure (refused, unreachable, timeout)
//     reports connectionError and then disconnected, with no retries
//   * a drop after a successful connect reports connectionLost and retries with
//     backoff until it reconnects or runs out of attempts (then disconnected)
//   * disconnect() is silent and synchronous: nothing the socket emits while we
//     tear it down is turned into a lost-connection or a retry
class TcpTransport : public QObject {
    Q_OBJECT

  public:
    explicit TcpTransport(QObject* parent = nullptr);
    ~TcpTransport() override;

    [[nodiscard]] bool connect(const QString& host, int port);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

    bool send(const QByteArray& data);

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] int port() const noexcept { return m_port; }

    void setConnectionTimeout(int ms) { m_connectionTimeoutMs = ms; }
    void setReconnectEnabled(bool enabled) { m_reconnectEnabled = enabled; }
    void setMaxReconnectAttempts(int attempts) { m_maxReconnectAttempts = attempts; }
    // base of the exponential backoff between reconnect attempts
    void setReconnectDelay(int ms) { m_reconnectDelayMs = ms; }

  signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void connectionLost();
    void dataReceived(const QByteArray& data);
    void reconnecting(int attempt, int maxAttempts);

  private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void onConnectionTimeout();
    void onReconnectAttempt();

  private:
    // an attempt (initial or reconnect) did not end in a connection
    void failAttempt(const QString& error);
    void scheduleReconnect();
    void tearDown();

    QTcpSocket m_socket;
    QString m_host;
    int m_port = 0;

    QTimer m_connectionTimer;
    int m_connectionTimeoutMs = 5000;

    QTimer m_reconnectTimer;
    bool m_reconnectEnabled = true;
    int m_reconnectAttempts = 0;
    int m_maxReconnectAttempts = 3;
    int m_reconnectDelayMs = 1000;

    // connected at least once since connect(): a later drop is a lost link
    // worth retrying, an early failure is not
    bool m_wasConnected = false;

    // set by disconnect(); the socket's own signals are ignored until the next
    // connect() so a teardown we asked for never looks like a lost link
    bool m_closing = false;
};

} // namespace OpenMix
