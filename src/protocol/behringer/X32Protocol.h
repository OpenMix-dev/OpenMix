#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/OscTransport.h"
#include <QDateTime>
#include <QMap>
#include <QTimer>

namespace OpenMix {

// pending request tracking for timeout handling
struct X32PendingRequest {
    QString path;
    QDateTime timestamp;
    ParameterCallback callback;
    qint64 sentTime;
};

// Behringer X32 / Midas M32 OSC Protocol implementation
class X32Protocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit X32Protocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~X32Protocol() override;

    // protocol identification
    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override {
        return m_capabilities.displayName + " OSC Protocol";
    }

    // connection management
    [[nodiscard]] bool connect(const QString& host, int port) override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const override { return m_connectionState == ConnectionState::Connected; }
    [[nodiscard]] QString connectionStatus() const override { return m_statusMessage; }
    [[nodiscard]] ConnectionState connectionState() const override { return m_connectionState; }

    // parameter operations
    void sendParameter(const QString& path, const QVariant& value) override;
    [[nodiscard]] QVariant getParameter(const QString& path) override;
    void requestParameter(const QString& path) override;
    void requestParameterAsync(const QString& path, ParameterCallback callback) override;

    // snapshot operations
    void recallSnapshot(const Cue& cue) override;

    // scene/snapshot recall
    void recallScene(int sceneNumber) override;

    // semantic channel setters (used by actor-voice recall and fades)
    void setChannelFader(int channel, double level) override;
    void setChannelMute(int channel, bool muted) override;
    void setChannelPreamp(int channel, double gainDb) override;
    void setChannelHpf(int channel, bool on, double freqHz) override;
    void setChannelEqOn(int channel, bool on) override;
    void setChannelEqBand(int channel, int band, bool on, int type, double freqHz, double gainDb,
                          double q) override;
    void setChannelDynamics(int channel, bool on, double thresholdDb, double ratio, double attackMs,
                            double releaseMs, double makeupDb) override;

    // scribble strips
    void setChannelName(int channel, const QString& name) override;
    void setChannelColour(int channel, int colour) override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    [[nodiscard]] int latencyMs() const override { return m_latencyMs; }

    // capabilities
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }
    [[nodiscard]] bool supportsParameterFeedback() const override { return true; }

    // X32-specific: list of parameters to recall
    [[nodiscard]] QStringList snapshotParameters() const { return m_snapshotParams; }
    void setSnapshotParameters(const QStringList& params) { m_snapshotParams = params; }

    // configuration
    void setConnectionTimeout(int ms) { m_connectionTimeoutMs = ms; }
    void setRequestTimeout(int ms) { m_requestTimeoutMs = ms; }
    void setMaxReconnectAttempts(int attempts) { m_maxReconnectAttempts = attempts; }

  private slots:
    void onTransportConnected();
    void onTransportDisconnected();
    void onTransportError(const QString& error);
    void onMessageReceived(const QString& path, const QVariant& value);
    void onKeepAliveTimeout();
    void onConnectionTimeout();
    void onRequestTimeoutCheck();
    void onReconnectAttempt();

  private:
    void initializeSnapshotParams();
    void rebuildSnapshotParams();
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void handleXinfoResponse(const QVariant& value);
    void startReconnection();
    void updateLatency(qint64 roundTripMs);
    void processResponse(const QString& path, const QVariant& value);
    void parseMeters(const QByteArray& blob);

    MixerCapabilities m_capabilities;
    OscTransport m_transport;

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    // keep-alive timer (X32 requires /xremote every 10 seconds)
    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 8000; // 8s

    // connection timeout
    QTimer m_connectionTimer;
    int m_connectionTimeoutMs = 5000; // 5s

    // reconnection
    QTimer m_reconnectTimer;
    int m_reconnectAttempts = 0;
    int m_maxReconnectAttempts = 3;
    int m_reconnectDelayMs = 1000; // exponential backoff base

    // request timeout tracking
    QTimer m_requestTimeoutTimer;
    QMap<QString, X32PendingRequest> m_pendingRequests;
    int m_requestTimeoutMs = 2000; // 2s

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
};

} // namespace OpenMix
