#pragma once

#include "MixerCapabilities.h"
#include "MixerProtocol.h"
#include <QMap>
#include <QTimer>

namespace OpenMix {

// loopback protocol for testing without real hardware
class LoopbackProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit LoopbackProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~LoopbackProtocol() override = default;

    // protocol identification
    QString protocolName() const override { return m_capabilities.displayName; }
    QString protocolDescription() const override { return "Virtual loopback for testing"; }

    // connection management
    bool connect(const QString& host, int port) override;
    void disconnect() override;
    bool isConnected() const override { return m_connected; }
    QString connectionStatus() const override { return m_statusMessage; }
    ConnectionState connectionState() const override {
        return m_connected ? ConnectionState::Connected : ConnectionState::Disconnected;
    }

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

    // latency
    int latencyMs() const override { return 1; }

    // capabilities
    const MixerCapabilities& capabilities() const override { return m_capabilities; }

  private:
    void initializeDefaultState();

    MixerCapabilities m_capabilities;
    bool m_connected = false;
    QString m_statusMessage;
    QMap<QString, QVariant> m_parameterState;
};

} // namespace OpenMix
