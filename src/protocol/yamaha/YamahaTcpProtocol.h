#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QMap>
#include <QTimer>

namespace OpenMix {

// base class for Yamaha TCP-based protocols (TF, QL, CL, DM7)
// use text-based commands over TCP
// port 49280

// pending async request tracking
struct PendingRequest {
    QString path;
    ParameterCallback callback;
    qint64 timestamp;
};

class YamahaTcpProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit YamahaTcpProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~YamahaTcpProtocol() override;

    // protocol identification
    QString protocolName() const override { return m_capabilities.displayName; }
    QString protocolDescription() const override {
        return m_capabilities.displayName + " TCP Protocol";
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
    void recallSnapshot(const Cue& cue) override;

    // scene recall
    void recallScene(int sceneNumber) override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    int latencyMs() const override { return m_latencyMs; }

    // capabilities
    const MixerCapabilities& capabilities() const override { return m_capabilities; }

  protected:
    // command builders, can be overridden by subclasses for model-specific commands
    virtual QString buildDCAFaderCommand(int dca, float level);
    virtual QString buildDCAMuteCommand(int dca, bool muted);
    virtual QString buildSceneRecallCommand(int sceneNumber);
    virtual QString buildKeepAliveCommand();

    // parse incoming text data
    virtual void parseResponse(const QString& response);

    // subclass-specific init
    virtual void initializeSnapshotParams() = 0;

    void sendCommand(const QString& command);

    MixerCapabilities m_capabilities;
    TcpTransport m_transport;
    QMap<QString, QVariant> m_parameterCache;
    QStringList m_snapshotParams;

  private slots:
    void onTransportConnected();
    void onTransportDisconnected();
    void onTransportError(const QString& error);
    void onTransportConnectionLost();
    void onDataReceived(const QByteArray& data);
    void onKeepAliveTimeout();
    void onReconnecting(int attempt, int maxAttempts);
    void onRequestTimeoutCheck();

  private:
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void processLine(const QString& line);

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;

    // async request tracking
    QMap<QString, PendingRequest> m_pendingRequests;
    QTimer m_requestTimeoutTimer;
    static constexpr int REQUEST_TIMEOUT_MS = 2000;
    static constexpr int REQUEST_TIMEOUT_CHECK_INTERVAL = 500;
};

} // namespace OpenMix
