#pragma once

#include <QHash>
#include <QObject>

class QTimer;

namespace OpenMix {

// Per-channel silence/clip state for scribble-strip coloring.
enum class ChannelState {
    Normal = 0,   // audio present in band
    Silent = 1,   // below the silence floor for longer than the silence timeout
    Clipping = 2  // hit/exceeded the clip ceiling (held briefly, then restored)
};

// Watches per-channel meter levels and raises a scribble-strip state per channel.
// Feed it metering via onLevel(); it runs a small state machine per channel:
//   - level >= clipThreshold        -> Clipping, held for clipHold, then restored
//   - level <= silenceThreshold for -> Silent (after silenceTimeout elapses)
//     longer than silenceTimeout
//   - otherwise                     -> Normal
// Timeouts/thresholds are settable so tests (and rooms) can tune responsiveness.
class ChannelMonitor : public QObject {
    Q_OBJECT

  public:
    explicit ChannelMonitor(QObject* parent = nullptr);

    void setSilenceThreshold(double level) { m_silenceThreshold = level; }
    [[nodiscard]] double silenceThreshold() const noexcept { return m_silenceThreshold; }

    void setClipThreshold(double level) { m_clipThreshold = level; }
    [[nodiscard]] double clipThreshold() const noexcept { return m_clipThreshold; }

    // how long a channel must stay below the floor before it is declared Silent
    void setSilenceTimeoutMs(int ms) { m_silenceTimeoutMs = ms; }
    [[nodiscard]] int silenceTimeoutMs() const noexcept { return m_silenceTimeoutMs; }

    // how long Clipping is held after the last over-ceiling sample before restore
    void setClipHoldMs(int ms) { m_clipHoldMs = ms; }
    [[nodiscard]] int clipHoldMs() const noexcept { return m_clipHoldMs; }

    [[nodiscard]] ChannelState channelState(int channel) const;

    void reset(); // clear all per-channel state and timers

  public slots:
    void onLevel(int channel, double level);

  signals:
    void channelStateChanged(int channel, int state);

  private:
    struct ChannelData {
        ChannelState state = ChannelState::Normal;
        QTimer* silenceTimer = nullptr;
        QTimer* clipTimer = nullptr;
    };

    ChannelData& ensureChannel(int channel);
    void setState(int channel, ChannelState state);
    void onClipHoldExpired(int channel);

    double m_silenceThreshold = 0.05;
    double m_clipThreshold = 0.97;
    int m_silenceTimeoutMs = 4000;
    int m_clipHoldMs = 1500;

    QHash<int, ChannelData> m_channels;
};

} // namespace OpenMix
