#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QTimer>
#include <QUdpSocket>

namespace OpenMix {

// base class for Allen & Heath "ACE" binary TCP protocols (Avantis, dLive), port
// 51321. This is a property protocol: frames are F0 .. F7, addresses are 2-byte
// handles the console hands back during a subscribe-by-name handshake, and level
// values are a 2-byte dB encoding. Control covers channel level/mute, DCA mute,
// and scene recall (the DCA *fader* plane is receive-only, as in the reference).
// Byte layouts recovered from the reference AvantisDriver/DLiveDriver.
//
// A session needs two sockets: the client advertises its UDP local port
// (E0 00 04 01 03 <port:2> E7), the console answers with its own
// (E0 00 04 02 03 <port:2> E7), and only then does the subscribe chain start.
// Metering arrives on that UDP socket and the keep-alive goes back out over it.
class AllenHeathTcpProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit AllenHeathTcpProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~AllenHeathTcpProtocol() override;

    // protocol identification
    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override {
        return m_capabilities.displayName + " ACE Protocol";
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

    // semantic setters
    void setChannelFaderDb(int channel, double level) override;
    void setChannelMute(int channel, bool muted) override;

    // keep-alive
    void refresh() override;

    // latency monitoring
    [[nodiscard]] int latencyMs() const override { return m_latencyMs; }

    // capabilities
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }

    // some ACE opcodes are firmware-gated (Avantis extends the op above a build
    // threshold; dLive extends for non-1.x software). The app sets these from the
    // discovered console's version so the exact opcode variant is emitted.
    void setFirmwareRevision(int rev) { m_firmwareRev = rev; }
    void setFirmwareVersion(const QString& version) { m_firmwareVersion = version; }

  protected:
    // per-console opcodes / plane offsets (differ between Avantis and dLive, and
    // are firmware-gated). Defaults are the base (older-firmware) opcodes.
    virtual quint8 channelLevelOp() const = 0; // Avantis 0x25/0x2B, dLive 0x30/0x38
    virtual quint8 channelMuteOp() const = 0;  // Avantis 0x26/0x2C, dLive 0x31/0x39
    virtual int channelMutePlane() const = 0;  // added to (ch-1): Avantis 0x20, dLive 0x80
    virtual int dcaMutePlane() const = 0;      // added to (dca-1): Avantis 0x18, dLive 0x20

    int m_firmwareRev = -1;    // build number from " - Rev. N"; -1 = unknown
    QString m_firmwareVersion; // full version string (for the dLive 1.x test)

    virtual void initializeSnapshotParams() = 0;

    // the ordered object names subscribed on connect (control-relevant subset);
    // the console returns a 2-byte handle per name. Overridable for GLD.
    virtual QList<QByteArray> subscribeObjects() const;

    // dB -> 2-byte encoding. Avantis/dLive use one lookup table; GLD uses a
    // sibling table (overridden). Shared logic in encodeDbTables().
    virtual QByteArray encodeDb(double dB) const;
    static QByteArray encodeDbTables(double dB, const quint8 posLo[10], const quint8 negLo[10]);

    // frame builders (protected for tests). Handle is the 2-byte object handle.
    static QByteArray buildSubscribe(const QByteArray& objectName);

    static QByteArray buildSessionSeed(quint16 udpPort);
    static QByteArray buildKeepAlive();

    // the console's advertised UDP port; 0 if the frame is not a session reply
    static quint16 parseSessionReply(const QByteArray& frame);

    // the functional area a frame belongs to, from [3..4]
    static quint16 areaOf(const QByteArray& frame);

    // our source id, written at [5..6] of every frame we originate
    static constexpr quint16 SOURCE_ID = 0x0001;

    // inbound frames carry the functional area they belong to at [3..4]
    static constexpr quint16 AREA_SUBSCRIBE_REPLY = 0x0001;
    static constexpr quint16 AREA_METERING = 0x0006;
    static constexpr quint16 AREA_SCENE_CHANGED = 0x000B;

    static constexpr int ACE_HEADER_SIZE = 11;

    // A meter datagram carries one 8-byte record per channel, in channel order;
    // only the record's first byte is a level. 0 is -inf and 0xFE/0xFF mark a
    // channel the console is not metering, so both read as silence. The byte's dB
    // curve is unknown, so it passes through as a monotonic position.
    static constexpr int METER_RECORD_SIZE = 8;
    static constexpr int METER_CHANNELS = 128;
    static constexpr quint8 METER_INVALID = 0xFE;
    static constexpr int METER_MIN_DATAGRAM = 200;

    // the 1-based scene the console switched to; 0 if the frame is not a scene change
    static int parseSceneChanged(const QByteArray& frame);
    QByteArray buildChannelLevel(const QByteArray& handle, int channel, double dB) const;
    QByteArray buildChannelMute(const QByteArray& handle, int channel, bool on) const;
    QByteArray buildDcaMute(const QByteArray& handle, int dca, bool on) const;
    static QByteArray buildSceneRecall(const QByteArray& handle, int sceneNumber);

    // dLive bank/spill addressing (op 0x12): channel index is bank-relative
    // ((ch-1)&7)+0x80 for level, +0x98 for mute, against a per-8-channel handle
    QByteArray buildChannelLevelSpill(const QByteArray& bankHandle, int channel, double dB) const;
    QByteArray buildChannelMuteSpill(const QByteArray& bankHandle, int channel, bool on) const;

    // object names whose handles the control path needs
    static const QByteArray kInputMixer;
    static const QByteArray kDcaLevelsAndMutes;
    static const QByteArray kSceneManager;
    static const QByteArray kMeteringSources; // subscribing starts the UDP meter stream

    MixerCapabilities m_capabilities;
    TcpTransport m_transport;
    QMap<QString, QVariant> m_parameterCache;
    QStringList m_snapshotParams;

    // learned object handles (object name -> 2-byte handle)
    QMap<QByteArray, QByteArray> m_handles;

  private slots:
    void onTransportConnected();
    void onTransportDisconnected();
    void onTransportError(const QString& error);
    void onTransportConnectionLost();
    void onDataReceived(const QByteArray& data);
    void onKeepAliveTimeout();
    void onRxWatchdogTimeout();
    void onHandshakeTimeout();
    void onReconnecting(int attempt, int maxAttempts);
    void onUdpDataReceived();

  private:
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void startHandshake();
    void sendNextSubscribe();
    void handleAceFrame(const QByteArray& frame);
    void handleControlFrame(const QByteArray& frame);
    void handleMeterDatagram(const QByteArray& datagram);
    void failHandshake(const QString& reason);
    QByteArray handleFor(const QByteArray& objectName) const { return m_handles.value(objectName); }

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    // metering in, keep-alive out
    QUdpSocket m_udpSocket;
    quint16 m_consoleUdpPort = 0;

    QTimer m_keepAliveTimer;
    QTimer m_rxWatchdogTimer;
    QTimer m_handshakeTimer;
    QElapsedTimer m_lastRxTimer;

    // the console drops a session that goes silent for 1.5 s; any inbound byte
    // counts as alive
    static constexpr int KEEPALIVE_INTERVAL = 1000;
    static constexpr int RX_WATCHDOG_INTERVAL = 500;
    static constexpr int RX_SILENCE_LIMIT = 1500;
    static constexpr int HANDSHAKE_TIMEOUT = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;

    // subscribe handshake progress
    QList<QByteArray> m_subscribeQueue;
    int m_subscribeIndex = 0;
    bool m_handshakeComplete = false;
    bool m_sessionOpen = false;
};

} // namespace OpenMix
