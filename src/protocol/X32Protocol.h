#pragma once

#include "MixerProtocol.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>
#include <QTimer>
#include <QUdpSocket>

#ifndef LIBLO_DISABLED
#include <lo/lo.h>
#endif

namespace StageBlend {

// pending request tracking for timeout handling
struct PendingRequest {
    QString path;
    QDateTime timestamp;
    ParameterCallback callback;
    qint64 sentTime; // for latency measurement
};

class X32Protocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit X32Protocol(QObject* parent = nullptr);
    ~X32Protocol() override;

    // protocol identification
    QString protocolName() const override { return "X32/M32"; }
    QString protocolDescription() const override {
        return "Behringer X32 / Midas M32 OSC Protocol";
    }

    // connection management
    bool connect(const QString& host, int port) override;
    void disconnect() override;
    bool isConnected() const override { return m_connectionState == ConnectionState::Connected; }
    QString connectionStatus() const override { return m_statusMessage; }
    ConnectionState connectionState() const override { return m_connectionState; }

    // parameter operations
    void sendParameter(const QString& path, const QVariant& value) override;
    QVariant getParameter(const QString& path) override;
    void requestParameter(const QString& path) override;
    void requestParameterAsync(const QString& path, ParameterCallback callback) override;

    // snapshot operations
    void captureSnapshot(Cue& cue) override;
    void recallSnapshot(const Cue& cue) override;
    QJsonObject captureCurrentState() override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    int latencyMs() const override { return m_latencyMs; }

    // x32-specific: list of parameters to capture
    QStringList snapshotParameters() const { return m_snapshotParams; }
    void setSnapshotParameters(const QStringList& params) { m_snapshotParams = params; }

    // configuration
    void setConnectionTimeout(int ms) { m_connectionTimeoutMs = ms; }
    void setRequestTimeout(int ms) { m_requestTimeoutMs = ms; }
    void setMaxReconnectAttempts(int attempts) { m_maxReconnectAttempts = attempts; }

  private slots:
    void onReadyRead();
    void onKeepAliveTimeout();
    void onConnectionTimeout();
    void onRequestTimeoutCheck();
    void onReconnectAttempt();

  private:
    void sendOscMessage(const QString& path);
    void sendOscMessage(const QString& path, float value);
    void sendOscMessage(const QString& path, int value);
    void sendOscMessage(const QString& path, const QString& value);
    void parseOscMessage(const QByteArray& data);
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void handleXinfoResponse(const QVariant& value);
    void startReconnection();
    void updateLatency(qint64 roundTripMs);
    void processResponse(const QString& path, const QVariant& value);

#ifndef LIBLO_DISABLED
    // liblo server for receiving
    lo_server_thread m_oscServer = nullptr;
    lo_address m_oscAddress = nullptr;
#endif

    // qt UDP socket (alternative to liblo for sending)
    QUdpSocket m_socket;
    QString m_host;
    int m_port = 10023;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    // keep-alive timer (X32 requires /xremote every 10 seconds)
    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 8000; // 8 seconds

    // connection timeout
    QTimer m_connectionTimer;
    int m_connectionTimeoutMs = 5000; // 5 seconds

    // reconnection
    QTimer m_reconnectTimer;
    int m_reconnectAttempts = 0;
    int m_maxReconnectAttempts = 3;
    int m_reconnectDelayMs = 1000; // exponential backoff base

    // request timeout tracking
    QTimer m_requestTimeoutTimer;
    QMap<QString, PendingRequest> m_pendingRequests;
    int m_requestTimeoutMs = 2000; // 2 seconds

    // latency monitoring
    int m_latencyMs = 0;
    QList<int> m_latencyHistory;
    static constexpr int LATENCY_HISTORY_SIZE = 10;

    // cached parameters
    QMap<QString, QVariant> m_parameterCache;
    QStringList m_snapshotParams;

    // last keep-alive response tracking
    qint64 m_lastResponseTime = 0;
    bool m_waitingForXinfo = false;

#ifndef LIBLO_DISABLED
    // OSC callback (static for liblo)
    static int oscHandler(const char* path, const char* types, lo_arg** argv, int argc,
                          lo_message msg, void* user_data);
#endif
};

} // namespace StageBlend
