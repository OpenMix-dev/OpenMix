#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/TcpTransport.h"
#include <QByteArray>
#include <QMap>
#include <QTimer>

namespace OpenMix {

// DiGiCo SD-series console driver.
//
// Speaks the console's TLV control protocol over TCP port 51321. Frames carry a
// 4-character block tag (MMIX mixing, MSUP setup, MPRO property, EEVT events),
// a big-endian length, a fixed 0x11 marker, an inner-length byte, a 4-byte
// opcode, then a run of type/length/value elements (0x31 string, 0x14 int32,
// 0x11/0x01 byte). Channel indices are zero-based; fader/gain values are carried
// as centidB (dB x 100) in a big-endian signed int32.
//
// The wire format was reverse-engineered from a reference implementation and has
// not been validated against physical hardware. It is offered as a best-effort
// driver for the SD range; the framing, opcodes, and value scaling below reflect
// what could be recovered.
class DiGiCoProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit DiGiCoProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~DiGiCoProtocol() override;

    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override {
        return m_capabilities.displayName + " SD Protocol";
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

    // frame construction (exposed for tests)
    static QByteArray buildFrame(const char* tag, const QByteArray& opcode,
                                 const QByteArray& elements);
    static QByteArray stringElement(const QString& text);
    static QByteArray int32Element(qint32 value);
    static QByteArray byteElement(quint8 value);

    // semantic setters (exposed for tests)
    QByteArray buildFaderMessage(const QString& objectPath, double db);
    QByteArray buildMuteMessage(const QString& objectPath, bool muted);
    QByteArray buildSubscribeMixing();
    QByteArray buildSetClientType();

  protected:
    virtual void parseProtocolData(const QByteArray& data);

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
    void sendHandshake();

    MixerCapabilities m_capabilities;
    TcpTransport m_transport;
    QMap<QString, QVariant> m_parameterCache;

    QString m_host;
    int m_port = 0;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;

    QTimer m_keepAliveTimer;
    static constexpr int KEEPALIVE_INTERVAL = 5000;

    int m_latencyMs = 0;
    QByteArray m_receiveBuffer;
};

} // namespace OpenMix
