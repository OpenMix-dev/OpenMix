#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QTimer>

namespace OpenMix {

// base class for Allen & Heath "ACE" binary TCP protocols (Avantis, dLive), port
// 51321. This is a property protocol: frames are F0 .. F7, addresses are 2-byte
// handles the console hands back during a subscribe-by-name handshake, and level
// values are a 2-byte dB encoding. Control covers channel level/mute, DCA mute,
// and scene recall (the DCA *fader* plane is receive-only, as in the reference).
// Byte layouts recovered from the reference AvantisDriver/DLiveDriver.
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
    void setChannelFader(int channel, double level) override;
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
    QByteArray buildChannelLevel(const QByteArray& handle, int channel, double dB) const;
    QByteArray buildChannelMute(const QByteArray& handle, int channel, bool on) const;
    QByteArray buildDcaMute(const QByteArray& handle, int dca, bool on) const;
    static QByteArray buildSceneRecall(const QByteArray& handle, int sceneNumber);

    // dLive bank/spill addressing (op 0x12): channel index is bank-relative
    // ((ch-1)&7)+0x80 for level, +0x98 for mute, against a per-8-channel handle
    QByteArray buildChannelLevelSpill(const QByteArray& bankHandle, int channel, double dB) const;
    QByteArray buildChannelMuteSpill(const QByteArray& bankHandle, int channel, bool on) const;

    // maps a normalized 0..1 fader position to dB for the encoder
    static double dbFromLevel(double level);

    // object names whose handles the control path needs
    static const QByteArray kInputMixer;
    static const QByteArray kDcaLevelsAndMutes;
    static const QByteArray kSceneManager;

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
    void onReconnecting(int attempt, int maxAttempts);

  private:
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);
    void startHandshake();
    void sendNextSubscribe();
    void handleAceFrame(const QByteArray& frame);
    QByteArray handleFor(const QByteArray& objectName) const { return m_handles.value(objectName); }

    QString m_host;
    int m_port;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;

    // subscribe handshake progress
    QList<QByteArray> m_subscribeQueue;
    int m_subscribeIndex = 0;
    bool m_handshakeComplete = false;
};

} // namespace OpenMix
