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

    // The console's MIDI channel (Utility > General > MIDI on SQ/Qu, Setup /
    // Control on GLD) rides the status byte of every message and cannot be read
    // back: a mismatch is a connected desk that ignores everything. 1-16.
    void setMidiChannel(int channel1To16);
    [[nodiscard]] int midiChannel() const { return m_midiChannel + 1; }

    // read back a level the console reported, in dB
    [[nodiscard]] std::optional<double> readChannelFader(int channel) override;

  protected:
    // dB -> the console's 14-bit NRPN level, through the active fader law, and
    // back for what the console reports
    [[nodiscard]] quint16 encodeLevel14(double dB) const;
    [[nodiscard]] double decodeLevel14(quint16 value) const;
    static quint16 encodeLinearTaper(double dB);
    static quint16 encodeAudioTaper(double dB);
    static double decodeLinearTaper(quint16 value);
    static double decodeAudioTaper(quint16 value);

    // the dB value standing in for -inf; anything at or below encodes to zero
    static constexpr double NEG_INF_DB = OpenMix::NEG_INF_DB;

    // SQ NRPN parameter MSB; the LSB is the 0-based item index unless noted.
    static constexpr int CH_LEVEL_TO_LR_MSB = 0x40; // input N -> LR level (LSB = N-1)
    static constexpr int CH_MUTE_MSB = 0x00;        // input N mute        (LSB = N-1)
    static constexpr int DCA_LEVEL_MSB = 0x4F;      // DCA N level         (LSB = 0x20 + N-1)
    static constexpr int DCA_LEVEL_LSB_BASE = 0x20; // SQ Iss5 p24 / Qu Iss2 p25: DCA1 = 4F 20
    static constexpr int DCA_MUTE_MSB = 0x02;       // DCA N mute          (LSB = N-1)

    // MIDI message builders used by subclasses; every one carries m_midiChannel
    QByteArray buildNRPNMessage(int nrpnMsb, int nrpnLsb, int valueMsb, int valueLsb) const;
    virtual QByteArray buildSceneRecall(int sceneNumber);
    QByteArray buildControlChange(int cc, int value) const;

    // parse incoming MIDI data
    virtual void parseMidiData(const QByteArray& data);

    // one decoded NRPN (msb/lsb = parameter, data = 14-bit value halves; dataLsb
    // is -1 when the console sent only three messages). The base reads the SQ
    // map; the channel-in-MSB families (Qu-16/24/32, GLD) override.
    virtual void handleNrpn(int msb, int lsb, int dataMsb, int dataLsb);

    // a Note On on the console's channel: the mute feedback of the
    // channel-in-MSB families. Default ignores it.
    virtual void handleNoteOn(int note, int velocity);

    // publish a value the console reported
    void reportParameter(const QString& path, const QVariant& value);

    FaderLaw m_faderLaw = FaderLaw::LinearTaper;
    int m_midiChannel = 0; // 0-based, as it goes on the wire; console channel 1 by default

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
    void onNrpnFlush();

  private:
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void processControlChange(int channel, int cc, int value);
    void finishNrpn();
    void processSysEx(const QByteArray& sysex);

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;

    // NRPN tracking for multi-message sequences. A sequence is finished by its
    // data LSB, by the start of the next sequence, or by this timer for the
    // three-message form (GLD faders) that never sends one.
    NRPNState m_nrpnState;
    QTimer m_nrpnFlushTimer;
    static constexpr int NRPN_TIMEOUT_MS = 100; // max time between NRPN messages
};

} // namespace OpenMix
