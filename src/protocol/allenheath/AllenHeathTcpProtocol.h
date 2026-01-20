#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QByteArray>
#include <QMap>
#include <QTimer>

namespace OpenMix {

// base class for Allen & Heath binary TCP protocols (Avantis, dLive)
// use a proprietary binary protocol (ACE Protocol)
// port 51321
class AllenHeathTcpProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit AllenHeathTcpProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~AllenHeathTcpProtocol() override;

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
    void captureSnapshot(Cue& cue) override;
    void recallSnapshot(const Cue& cue) override;
    QJsonObject captureCurrentState() override;

    // scene recall
    void recallScene(int sceneNumber) override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    int latencyMs() const override { return m_latencyMs; }

    // capabilities
    const MixerCapabilities& capabilities() const override { return m_capabilities; }

  protected:
    // binary message builders
    QByteArray buildDCAFaderMessage(int dca, float level);
    QByteArray buildDCAMuteMessage(int dca, bool muted);
    QByteArray buildSceneRecallMessage(int sceneNumber);
    QByteArray buildKeepAliveMessage();

    // parse incoming binary data
    virtual void parseProtocolData(const QByteArray& data);

    // subclass-specific initialization
    virtual void initializeSnapshotParams() = 0;

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

  private:
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;
};

} // namespace OpenMix
