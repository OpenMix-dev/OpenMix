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

    QString protocolName() const override { return "Yamaha OSC"; }
    QString protocolDescription() const override { return "Yamaha OSC/UDP Protocol"; }

    bool connect(const QString& host, int port) override;
    void disconnect() override;
    bool isConnected() const override { return m_connectionState == ConnectionState::Connected; }
    QString connectionStatus() const override { return m_statusMessage; }
    ConnectionState connectionState() const override { return m_connectionState; }

    void sendParameter(const QString& path, const QVariant& value) override;
    QVariant getParameter(const QString& path) override;
    void requestParameter(const QString& path) override;
    void requestParameterAsync(const QString& path, ParameterCallback callback) override;

    void recallSnapshot(const Cue& cue) override;
    void recallScene(int sceneNumber) override;
    void refresh() override;

    int latencyMs() const override { return m_latencyMs; }
    const MixerCapabilities& capabilities() const override { return m_capabilities; }

  protected:
    virtual void initializeSnapshotParams();
    void rebuildSnapshotParams();

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

  private:
    void sendOscMessage(const QString& path);
    void sendOscMessage(const QString& path, float value);
    void sendOscMessage(const QString& path, int value);
    void sendOscMessage(const QString& path, const QString& value);
    void parseOscMessage(const QByteArray& data);
    QVariant parseOscArgument(const QByteArray& data, int& offset, char type);
    QByteArray buildOscMessage(const QString& path);
    QByteArray buildOscMessage(const QString& path, float value);
    QByteArray buildOscMessage(const QString& path, int value);
    QByteArray buildOscMessage(const QString& path, const QString& value);

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
