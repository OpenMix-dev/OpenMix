#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/OscTransport.h"
#include <QDateTime>
#include <QMap>
#include <QTimer>

namespace OpenMix {

// pending request tracking
struct WingPendingRequest {
    QString path;
    QDateTime timestamp;
    ParameterCallback callback;
    qint64 sentTime;
};

// Behringer WING OSC protocol implementation
// WING uses OSC but with different namespace than X32:
// DCA paths: /dca/1-24/fader, /dca/1-24/mute
// scene recall: /action/scenes/recall,i with scene number
// defaults to port 2223
class WingProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit WingProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~WingProtocol() override;

    // protocol identification
    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override { return "Behringer WING OSC Protocol"; }

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

    // scene recall
    void recallScene(int sceneNumber) override;

    // snippet (partial scene) recall
    void recallSnippet(int snippetNumber) override;

    // semantic channel setters (WING uses real-world values, so most pass through)
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
    void setChannelColor(int channel, int color) override;

    void setDcaMute(int dca, bool muted) override;
    void setDcaFader(int dca, double level) override;
    void setDcaName(int dca, const QString& name) override;
    void setChannelDcaMask(int channel, quint32 mask) override;
    void setBusDcaMask(int bus, quint32 mask) override;
    [[nodiscard]] std::optional<quint32> readChannelDcaMask(int channel) override;
    [[nodiscard]] std::optional<quint32> readBusDcaMask(int bus) override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    [[nodiscard]] int latencyMs() const override { return m_latencyMs; }

    // capabilities
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }
    [[nodiscard]] bool supportsParameterFeedback() const override { return true; }

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
    void requestDcaMembership();
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void handleInfoResponse(const QVariant& value);
    void startReconnection();
    void updateLatency(qint64 roundTripMs);
    void processResponse(const QString& path, const QVariant& value);

    MixerCapabilities m_capabilities;
    OscTransport m_transport;

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    // keep-alive timer (WING requires periodic messages)
    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 8000;

    // connection timeout
    QTimer m_connectionTimer;
    int m_connectionTimeoutMs = 5000;

    // reconnection
    QTimer m_reconnectTimer;
    int m_reconnectAttempts = 0;
    int m_maxReconnectAttempts = 3;
    int m_reconnectDelayMs = 1000;

    // request timeout tracking
    QTimer m_requestTimeoutTimer;
    QMap<QString, WingPendingRequest> m_pendingRequests;
    int m_requestTimeoutMs = 2000;

    // latency monitoring
    int m_latencyMs = 0;
    QList<int> m_latencyHistory;
    static constexpr int LATENCY_HISTORY_SIZE = 10;

    // cached parameters
    QMap<QString, QVariant> m_parameterCache;
    QStringList m_snapshotParams;

    qint64 m_lastResponseTime = 0;
    bool m_waitingForInfo = false;
};

} // namespace OpenMix
