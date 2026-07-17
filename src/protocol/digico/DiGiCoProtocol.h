#pragma once

#include "../MixerCapabilities.h"
#include "../MixerProtocol.h"
#include "../transport/OscTransport.h"
#include <QMap>
#include <QString>

namespace OpenMix {

// DiGiCo SD / Quantum series, over the console's Generic OSC.
//
// DiGiCo publishes no address map: Generic OSC is user-defined, the syntax varies
// by model and software version, and the console only acts on it with External
// Control enabled. So the driver is a template - it sends the operator's patterns
// and nothing for an operation they left blank, since a guessed address would
// look like it worked here and do nothing on the desk.
class DiGiCoProtocol : public MixerProtocol {
    Q_OBJECT

  public:
    explicit DiGiCoProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);
    ~DiGiCoProtocol() override;

    [[nodiscard]] QString protocolName() const override { return m_capabilities.displayName; }
    [[nodiscard]] QString protocolDescription() const override {
        return m_capabilities.displayName + " Generic OSC";
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

    void setChannelFaderDb(int channel, double dB) override;
    void setChannelMute(int channel, bool muted) override;

    void refresh() override;
    [[nodiscard]] int latencyMs() const override { return m_latencyMs; }
    [[nodiscard]] const MixerCapabilities& capabilities() const override { return m_capabilities; }

    // The addresses this console listens on, as the operator entered them. "*" in
    // a pattern is replaced with the channel number. An empty pattern disables
    // that operation outright.
    struct OscTemplates {
        QString channelFader;
        QString channelMute;
        QString sceneRecall;
    };
    void setTemplates(const OscTemplates& templates) { m_templates = templates; }
    [[nodiscard]] const OscTemplates& templates() const { return m_templates; }

    // "/ch/*/fader" + channel 3 -> "/ch/3/fader"; empty in, empty out
    static QString expand(const QString& pattern, int channel);

  private:
    void setStatus(const QString& status);
    void setConnectionState(ConnectionState state);

    MixerCapabilities m_capabilities;
    OscTransport m_transport;
    OscTemplates m_templates;

    QString m_host;
    int m_port = 0;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_statusMessage;
    int m_latencyMs = 0;
    QMap<QString, QVariant> m_parameterCache;
};

} // namespace OpenMix
