#pragma once

#include "MixerCapabilities.h"
#include "MixerProtocol.h"
#include <QMap>

namespace OpenMix {

// loopback protocol for testing without real hardware
class LoopbackProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit LoopbackProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~LoopbackProtocol() override = default;

    // protocol identification
    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override { return "Virtual loopback for testing"; }

    // connection management
    [[nodiscard]] bool connect(const QString& host, int port) override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override {
        return m_connectionState == ConnectionState::Connected;
    }
    [[nodiscard]] QString connectionStatus() const override { return m_statusMessage; }
    [[nodiscard]] ConnectionState connectionState() const noexcept override {
        return m_connectionState;
    }

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

    // latency
    [[nodiscard]] int latencyMs() const noexcept override { return 1; }

    // capabilities
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }

  private:
    void initializeDefaultState();
    void emitParameterChangedAsync(const QString& addr, const QVariant& val);

    MixerCapabilities m_capabilities;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;
    QMap<QString, QVariant> m_parameterState;
};

} // namespace OpenMix
