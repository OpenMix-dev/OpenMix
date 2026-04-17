#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QByteArray>
#include <QMap>
#include <QTimer>

namespace OpenMix {

// NRPN state for tracking multi-message sequences
struct NRPNState {
    int channel = -1;
    int nrpnMsb = -1;
    int nrpnLsb = -1;
    int dataMsb = -1;
    int dataLsb = -1;
    qint64 timestamp = 0;

    bool isComplete() const { return nrpnMsb >= 0 && nrpnLsb >= 0 && dataMsb >= 0; }
    void reset() {
        channel = -1;
        nrpnMsb = nrpnLsb = dataMsb = dataLsb = -1;
        timestamp = 0;
    }
};

// base class for Allen & Heath MIDI-over-TCP protocols (SQ, GLD)
// uses MIDI messages (including SysEx & NRPN) wrapped in TCP
class AllenHeathMidiProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit AllenHeathMidiProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~AllenHeathMidiProtocol() override;

    // protocol identification
    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override {
        return m_capabilities.displayName + " MIDI/TCP Protocol";
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

    // scene recall
    void recallScene(int sceneNumber) override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    [[nodiscard]] int latencyMs() const override { return m_latencyMs; }

    // capabilities
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }

  protected:
    // MIDI message builders used by subclasses
    QByteArray buildNRPNMessage(int channel, int nrpnMsb, int nrpnLsb, int valueMsb, int valueLsb);
    QByteArray buildSysExSceneRecall(int sceneNumber);
    QByteArray buildControlChange(int channel, int cc, int value);

    // parse incoming MIDI data
    virtual void parseMidiData(const QByteArray& data);

    // subclass-specific param mapping
    virtual void initializeSnapshotParams() = 0;
    virtual QString dcaFaderPath(int dca) const = 0;
    virtual QString dcaMutePath(int dca) const = 0;

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
    void processControlChange(int channel, int cc, int value);
    void processNRPNComplete();
    void processSysEx(const QByteArray& sysex);

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;

    // NRPN tracking for multi-message sequences
    NRPNState m_nrpnState;
    static constexpr int NRPN_TIMEOUT_MS = 100; // max time between NRPN messages
};

} // namespace OpenMix
