#pragma once

#include "../../core/LevelDb.h"
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

// base class for Allen & Heath MIDI-over-TCP protocols (SQ, Qu, GLD)
// uses MIDI messages (NRPN levels/mutes, Bank+Program scene recall) over TCP
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
    [[nodiscard]] bool isConnected() const override {
        return m_connectionState == ConnectionState::Connected;
    }
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

    // Input-channel fader level + mute via NRPN. Parameter numbers verified
    // against the A&H SQ MIDI Protocol Issue 5 reference tables: inputs-to-LR
    // level MSB 0x40, input mute MSB 0x00, DCA level MSB 0x4F / LSB 0x20+ (Mix
    // Sends "Control" table), DCA mute MSB 0x02 (all LSB = 0-based item index).
    void setChannelFaderDb(int channel, double dB) override;
    void setChannelMute(int channel, bool muted) override;

    // Which curve the console maps NRPN levels through, set at Utility > General >
    // MIDI > NRPN Fader Law and not readable over MIDI, so it has to be told: a
    // mismatch still moves the fader, just to the wrong dB. Linear is standard.
    enum class FaderLaw { LinearTaper, AudioTaper };
    void setFaderLaw(FaderLaw law) { m_faderLaw = law; }
    [[nodiscard]] FaderLaw faderLaw() const { return m_faderLaw; }

  protected:
    // dB -> the console's 14-bit NRPN level, through the active fader law
    [[nodiscard]] quint16 encodeLevel14(double dB) const;
    static quint16 encodeLinearTaper(double dB);
    static quint16 encodeAudioTaper(double dB);

    // the dB value standing in for -inf; anything at or below encodes to zero
    static constexpr double NEG_INF_DB = OpenMix::NEG_INF_DB;

    // SQ NRPN parameter MSB; the LSB is the 0-based item index unless noted.
    static constexpr int CH_LEVEL_TO_LR_MSB = 0x40; // input N -> LR level (LSB = N-1)
    static constexpr int CH_MUTE_MSB = 0x00;        // input N mute        (LSB = N-1)
    static constexpr int DCA_LEVEL_MSB = 0x4F;      // DCA N level         (LSB = 0x20 + N-1)
    static constexpr int DCA_LEVEL_LSB_BASE = 0x20; // SQ Iss5 p24 / Qu Iss2 p25: DCA1 = 4F 20
    static constexpr int DCA_MUTE_MSB = 0x02;       // DCA N mute          (LSB = N-1)

    // MIDI message builders used by subclasses
    QByteArray buildNRPNMessage(int channel, int nrpnMsb, int nrpnLsb, int valueMsb, int valueLsb);
    virtual QByteArray buildSceneRecall(int sceneNumber);
    QByteArray buildControlChange(int channel, int cc, int value);

    // parse incoming MIDI data
    virtual void parseMidiData(const QByteArray& data);

    FaderLaw m_faderLaw = FaderLaw::LinearTaper;

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
