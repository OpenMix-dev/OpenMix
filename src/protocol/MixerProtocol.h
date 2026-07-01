#pragma once

#include "MixerCapabilities.h"
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>

namespace OpenMix {

class Cue;

enum class ConnectionState { Disconnected, Connecting, Connected, Reconnecting };

using ParameterCallback =
    std::function<void(const QString& path, const QVariant& value, bool success)>;

class MixerProtocol : public QObject {
    Q_OBJECT

  public:
    explicit MixerProtocol(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MixerProtocol() = default;

    virtual QString protocolName() const = 0;
    virtual QString protocolDescription() const = 0;

    virtual bool connect(const QString& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual QString connectionStatus() const = 0;
    virtual ConnectionState connectionState() const = 0;

    virtual void sendParameter(const QString& path, const QVariant& value) = 0;
    virtual QVariant getParameter(const QString& path) = 0;
    virtual void requestParameter(const QString& path) = 0;
    virtual void requestParameterAsync(const QString& path, ParameterCallback callback) = 0;

    virtual void recallSnapshot(const Cue& cue) = 0;
    virtual void recallScene(int sceneNumber) = 0;

    // recall a console snippet (partial scene) by index. Default no-op so drivers
    // opt in; OSC drivers (X32/Wing) override with the console's snippet action.
    virtual void recallSnippet(int /*snippet*/) {}

    // semantic per-channel setters used by actor-voice recall and timed fades.
    // Default to no-op so drivers opt in; network OSC drivers (X32/Wing) override.
    // channel is 1-based; level is normalized 0..1; other units are real-world
    // (dB, Hz, ms) and the driver scales them to the console wire format.
    virtual void setChannelFader(int channel, double level);
    virtual void setChannelMute(int channel, bool muted);
    virtual void setChannelPreamp(int channel, double gainDb);
    virtual void setChannelHpf(int channel, bool on, double freqHz);
    virtual void setChannelEqOn(int channel, bool on);
    virtual void setChannelEqBand(int channel, int band, bool on, int type, double freqHz,
                                  double gainDb, double q);
    virtual void setChannelDynamics(int channel, bool on, double thresholdDb, double ratio,
                                    double attackMs, double releaseMs, double makeupDb);

    virtual void refresh() = 0;
    virtual int latencyMs() const = 0;
    virtual const MixerCapabilities& capabilities() const;

    // true if the driver can read parameter values back from the console (so cue
    // landing can be verified). Send-only drivers (e.g. Allen & Heath MIDI/ACE)
    // return false and are treated as "assumed landed" rather than drifted.
    [[nodiscard]] virtual bool supportsParameterFeedback() const { return false; }

  signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void connectionStatusChanged(const QString& status);
    void connectionStateChanged(ConnectionState state);
    void connectionLost();

    void parameterChanged(const QString& path, const QVariant& value);
    void requestTimeout(const QString& path);

    // live input-channel meter level (channel 1-based, level 0..1 linear). Emitted
    // by drivers that subscribe to console metering; feeds the ChannelMonitor.
    void channelMeter(int channel, float level);

    void latencyChanged(int ms);
    void sceneChanged(int sceneNumber);
};

} // namespace OpenMix
