#pragma once

#include "MixerCapabilities.h"
#include "MixerProtocol.h"
#include <QMap>
#include <QStringList>

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

    // semantic channel setters; recorded as readable strings for testing
    void setChannelFader(int channel, double level) override;
    void setChannelMute(int channel, bool muted) override;
    void setChannelPreamp(int channel, double gainDb) override;
    void setChannelHpf(int channel, bool on, double freqHz) override;
    void setChannelEqOn(int channel, bool on) override;
    void setChannelEqBand(int channel, int band, bool on, int type, double freqHz, double gainDb,
                          double q) override;
    void setChannelDynamics(int channel, bool on, double thresholdDb, double ratio, double attackMs,
                            double releaseMs, double makeupDb) override;

    void setDcaMute(int dca, bool muted) override;
    void setDcaFader(int dca, double level) override;
    void setDcaName(int dca, const QString& name) override;
    void setChannelDcaMask(int channel, quint32 mask) override;
    void setBusDcaMask(int bus, quint32 mask) override;
    [[nodiscard]] std::optional<quint32> readChannelDcaMask(int channel) override;
    [[nodiscard]] std::optional<quint32> readBusDcaMask(int bus) override;

    [[nodiscard]] QStringList recordedCalls() const { return m_recordedCalls; }
    void clearRecordedCalls() { m_recordedCalls.clear(); }

    // keep-alive
    void refresh() override;

    // latency
    [[nodiscard]] int latencyMs() const noexcept override { return 1; }

    // capabilities
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }
    [[nodiscard]] bool supportsParameterFeedback() const override { return true; }

  private:
    void initializeDefaultState();
    void emitParameterChangedAsync(const QString& addr, const QVariant& val);

    MixerCapabilities m_capabilities;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;
    QMap<QString, QVariant> m_parameterState;
    QStringList m_recordedCalls;
};

} // namespace OpenMix
