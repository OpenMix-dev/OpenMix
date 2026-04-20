#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/OscTransport.h"
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QTimer>
#include <QUdpSocket>

namespace OpenMix {

struct YamahaPendingRequest {
    QString path;
    QDateTime timestamp;
    qint64 sentTime = 0;
    ParameterCallback callback;
};

class YamahaProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit YamahaProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~YamahaProtocol() override;

    [[nodiscard]] QString protocolName() const override { return "Yamaha OSC"; }
    [[nodiscard]] QString protocolDescription() const override { return "Yamaha OSC/UDP Protocol"; }

    [[nodiscard]] bool connect(const QString& host, int port) override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const override { return m_connectionState == ConnectionState::Connected; }
    [[nodiscard]] QString connectionStatus() const override { return m_statusMessage; }
    [[nodiscard]] ConnectionState connectionState() const override { return m_connectionState; }

    void sendParameter(const QString& path, const QVariant& value) override;
    [[nodiscard]] QVariant getParameter(const QString& path) override;
    void requestParameter(const QString& path) override;
    void requestParameterAsync(const QString& path, ParameterCallback callback) override;

    void recallSnapshot(const Cue& cue) override;
    void recallScene(int sceneNumber) override;
    void refresh() override;

    [[nodiscard]] int latencyMs() const override { return m_latencyMs; }
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }

  protected:
    virtual void initializeSnapshotParams();
    void rebuildSnapshotParams();

    // appends per-channel fader/on, EQ, and effect-send snapshot parameters for
    // `channelCount` channels.  `channelPrefix` is the path stem (e.g. "/ch/"),
    // `channelFieldWidth` controls zero-padding (2 for CL/QL/TF, 3 for DM7), and
    // `maxEffectSends` is the model-specific send-bus ceiling applied on top of
    // m_capabilities.effectSendBuses.
    void appendEqSnapshotParams(QStringList& params, const QString& channelPrefix,
                                int channelCount, int channelFieldWidth = 2,
                                int maxEffectSends = 16) const;

    MixerCapabilities m_capabilities;
    QStringList m_snapshotParams;

    // Yamaha uses separate receive/transmit ports
    int m_receivePort = 8000;
    int m_transmitPort = 9000;

  private slots:
    void onReadyRead();
    void onKeepAliveTimeout();
    void onConnectionTimeout();
    void onRequestTimeoutCheck();
    void onReconnectAttempt();

  protected:
    void parseOscMessage(const QByteArray& data);

  private:
    void sendOscMessage(const QString& path);
    void sendOscMessage(const QString& path, float value);
    void sendOscMessage(const QString& path, int value);
    void sendOscMessage(const QString& path, const QString& value);
    QVariant parseOscArgument(const QByteArray& data, int& offset, char type);

    void processResponse(const QString& path, const QVariant& value);
    void handleModelResponse(const QVariant& value);
    void startReconnection();
    void updateLatency(qint64 roundTripMs);
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);

    QUdpSocket m_receiveSocket;
    lo_address m_oscAddress = nullptr;
    QString m_host;
    int m_port = 0;

    QTimer m_keepAliveTimer;
    QTimer m_connectionTimer;
    QTimer m_requestTimeoutTimer;
    QTimer m_reconnectTimer;

    QHash<QString, QVariant> m_parameterCache;
    QHash<QString, YamahaPendingRequest> m_pendingRequests;

    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    qint64 m_lastResponseTime = 0;
    int m_latencyMs = 0;
    QList<int> m_latencyHistory;

    int m_reconnectAttempts = 0;
    bool m_waitingForModel = false;

    static constexpr int KEEPALIVE_INTERVAL = 8000;
    static constexpr int LATENCY_HISTORY_SIZE = 10;

    int m_connectionTimeoutMs = 5000;
    int m_requestTimeoutMs = 2000;
    int m_maxReconnectAttempts = 5;
    int m_reconnectDelayMs = 1000;
};

} // namespace OpenMix
