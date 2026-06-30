#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QTimer>

namespace OpenMix {

// Yamaha SCP (Remote Control Protocol) driver.
//
// ASCII line protocol over TCP port 49280, used by CL / QL / Rivage / DM7
// consoles.  Commands are LF-terminated
// text lines:
//
//   set <address> <idx1> <idx2> <value>\n
//   get <address> <idx1> <idx2>\n
//   ssrecall_ex MIXER:Lib/Scene <n>\n
//
// idx1 is the 0-based channel index (console channel 1 -> 0).  idx2 selects a
// sub-element (0 for channel scalars, the EQ band index for EQ bands).  Numeric
// values are scaled integers, e.g. fader level is centi-dB: 0 dB = 0,
// +10 dB = 1000, -inf = -32768.  Responses and console-side changes arrive as
// lines beginning "OK"/"OKm"/"NOTIFY" and are parsed into parameterChanged().

struct YamahaPendingRequest {
    QString address;
    qint64 sentTime = 0;
    ParameterCallback callback;
};

class YamahaProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    // SCP listens on TCP 49280 on every CL/QL/Rivage/DM7 console.
    static constexpr int SCP_PORT = 49280;

    // Fader level constants in centi-dB (MIXER:Current/InCh/Fader/Level).
    static constexpr int FADER_MAX_CENTIDB = 1000;   // +10 dB (top of throw)
    static constexpr int FADER_MIN_CENTIDB = -13800; // -138 dB (bottom, non-inf)
    static constexpr int FADER_NEG_INF = -32768;     // -inf

    // SCP node addresses for input channels.  Public so callers/tests can reuse.
    static constexpr auto AddrFaderLevel = "MIXER:Current/InCh/Fader/Level";
    static constexpr auto AddrFaderOn = "MIXER:Current/InCh/Fader/On";
    static constexpr auto AddrHaGain = "MIXER:Current/InCh/Port/HA/Gain";
    static constexpr auto AddrHpfOn = "MIXER:Current/InCh/HPF/On";
    static constexpr auto AddrHpfFreq = "MIXER:Current/InCh/HPF/Freq";
    static constexpr auto AddrEqOn = "MIXER:Current/InCh/PEQ/On";
    static constexpr auto AddrEqBandBypass = "MIXER:Current/InCh/PEQ/Band/Bypass";
    static constexpr auto AddrEqBandFreq = "MIXER:Current/InCh/PEQ/Band/Freq";
    static constexpr auto AddrEqBandGain = "MIXER:Current/InCh/PEQ/Band/Gain";
    static constexpr auto AddrEqBandQ = "MIXER:Current/InCh/PEQ/Band/Q";
    static constexpr auto AddrDynaThreshold = "MIXER:Current/InCh/Dyna2/Threshold";
    static constexpr auto SceneLib = "MIXER:Lib/Scene";

    explicit YamahaProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~YamahaProtocol() override;

    [[nodiscard]] QString protocolName() const override { return "Yamaha SCP"; }
    [[nodiscard]] QString protocolDescription() const override {
        return "Yamaha SCP (Remote Control Protocol) over TCP 49280";
    }

    [[nodiscard]] bool connect(const QString& host, int port) override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const override {
        return m_connectionState == ConnectionState::Connected;
    }
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
    [[nodiscard]] bool supportsParameterFeedback() const override { return true; }

    // --- semantic input-channel setters. channel is 1-based (MixerProtocol
    //     contract); converted to the 0-based SCP index internally. ---
    void setChannelFader(int ch, double level) override; // level normalized 0..1
    void setChannelMute(int ch, bool muted) override;
    void setChannelPreamp(int ch, double gainDb) override;
    void setChannelHpf(int ch, bool on, double freqHz) override;
    void setChannelEqOn(int ch, bool on) override;
    void setChannelEqBand(int ch, int band, bool on, int type, double freqHz, double gainDb,
                          double q) override;
    void setChannelDynamics(int ch, bool on, double thresholdDb, double ratio, double attackMs,
                            double releaseMs, double makeupDb) override;

    // --- pure command builders: produce the exact SCP ASCII bytes (LF-terminated). ---
    [[nodiscard]] static QByteArray scpSet(const QString& address, int idx1, int idx2, int value);
    [[nodiscard]] static QByteArray scpSet(const QString& address, int idx1, int idx2,
                                           const QString& value);
    [[nodiscard]] static QByteArray scpGet(const QString& address, int idx1, int idx2);
    [[nodiscard]] static QByteArray scpSceneRecall(int sceneNumber);

    // --- pure value scaling helpers. ---
    [[nodiscard]] static int faderLevelToScp(double level0to1); // 0..1 -> centi-dB taper
    [[nodiscard]] static int dbToCentiDb(double db);            // dB    -> centi-dB
    [[nodiscard]] static int hzToScpFreq(double hz);            // Hz    -> SCP freq (Hz * 10)
    [[nodiscard]] static int qToScp(double q);                  // Q     -> SCP Q   (Q * 1000)

    // --- semantic command builders: the exact bytes each setter sends. ---
    [[nodiscard]] QByteArray buildChannelFader(int ch, double level) const;
    [[nodiscard]] QByteArray buildChannelMute(int ch, bool muted) const;
    [[nodiscard]] QByteArray buildChannelPreamp(int ch, double gainDb) const;
    [[nodiscard]] QByteArray buildChannelHpfOn(int ch, bool on) const;
    [[nodiscard]] QByteArray buildChannelHpfFreq(int ch, double freqHz) const;
    [[nodiscard]] QByteArray buildChannelEqOn(int ch, bool on) const;
    [[nodiscard]] QByteArray buildChannelEqBandBypass(int ch, int band, bool on) const;
    [[nodiscard]] QByteArray buildChannelEqBandFreq(int ch, int band, double freqHz) const;
    [[nodiscard]] QByteArray buildChannelEqBandGain(int ch, int band, double gainDb) const;
    [[nodiscard]] QByteArray buildChannelEqBandQ(int ch, int band, double q) const;
    [[nodiscard]] QByteArray buildChannelDynamicsThreshold(int ch, double thresholdDb) const;

  protected:
    // Head-amp (preamp) gain scaling.  CL/QL/Rivage transmit centi-dB; the DM7
    // subclass overrides this to send whole-dB values.
    [[nodiscard]] virtual int preampGainToScp(double gainDb) const;

    // Parse one already-split SCP response/notify line (no trailing newline).
    void parseScpLine(const QByteArray& line);

    MixerCapabilities m_capabilities;
    TcpTransport m_transport;

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
    void sendCommand(const QByteArray& cmd);
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void updateLatency(qint64 roundTripMs);

    QString m_host;
    int m_port = SCP_PORT;

    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QByteArray m_rxBuffer;
    QHash<QString, QVariant> m_parameterCache;
    QHash<QString, YamahaPendingRequest> m_pendingRequests;

    QTimer m_keepAliveTimer;
    QTimer m_requestTimeoutTimer;

    qint64 m_lastResponseTime = 0;
    int m_latencyMs = 0;

    static constexpr int KEEPALIVE_INTERVAL = 5000;
    static constexpr int REQUEST_TIMEOUT_MS = 2000;
    static constexpr int MAX_RX_BUFFER = 65536;
};

} // namespace OpenMix
