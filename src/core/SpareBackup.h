#pragma once

#include <QJsonObject>
#include <QObject>

namespace OpenMix {

// Spare-backup mic model: one reserved console channel that can be allocated to
// cover a failed input, then switched live so the covered actor's backup voice
// plays through the spare. Mirrors the allocate / switch-now / switch-later
// workflow. State only -- applying the voice to the mixer is the caller's job.
class SpareBackup : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Inactive, // primary channel live, spare idle
        Armed,    // switch queued for the next cue fire
        Active    // spare is live, covering the allocated channel
    };

    explicit SpareBackup(QObject* parent = nullptr);

    // the console channel reserved as the spare mic (-1 = none configured)
    [[nodiscard]] int spareChannel() const noexcept { return m_spareChannel; }
    void setSpareChannel(int channel);

    // the channel the spare currently covers (-1 = unallocated)
    [[nodiscard]] int allocatedChannel() const noexcept { return m_allocatedChannel; }
    [[nodiscard]] bool isAllocated() const noexcept { return m_allocatedChannel >= 0; }

    // allocate the spare to cover a channel. Fails if no spare is configured or
    // the spare is currently active (can't reallocate a live spare).
    bool allocateTo(int channel);
    void removeAllocation();

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] bool isActive() const noexcept { return m_state == State::Active; }

    void switchNow();   // allocated -> Active immediately
    void switchLater(); // allocated -> Armed (promotes on the next cue fire)
    void revert();      // back to Inactive

  public slots:
    // promotes an Armed switch to Active; no effect otherwise
    void onCueFired();

  public:
    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed();
    void stateChanged(State state);
    void allocationChanged(int channel);

  private:
    void setState(State state);

    int m_spareChannel = -1;
    int m_allocatedChannel = -1;
    State m_state = State::Inactive;
};

} // namespace OpenMix
