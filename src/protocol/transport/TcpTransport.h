#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace OpenMix {

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
    void startReconnection();

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

    bool m_wasConnected = false;
};

} // namespace OpenMix
