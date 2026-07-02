#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace OpenMix {

// One EQ band of a channel voice. Values are real-world units; the mixer driver
// scales them to the console's wire format.
struct EqBand {
    int band = 1;         // 1-based band index
    bool on = true;       //
    int type = 0;         // console EQ type enum (PEQ / shelf / ...), driver-mapped
    double freq = 1000.0; // Hz
    double gain = 0.0;    // dB
    double q = 2.0;       //

    QJsonObject toJson() const;
    [[nodiscard]] static EqBand fromJson(const QJsonObject& json);
};

// A channel "voice" — the per-channel processing that makes up an actor's sound:
// preamp gain, HPF, EQ and dynamics. Every field is optional so a partial voice
// only touches the parameters it sets (matches DCAOverride semantics).
struct VoiceData {
    std::optional<double> gainDb; // preamp / head-amp gain
    std::optional<bool> hpfOn;
    std::optional<double> hpfFreq; // Hz
    std::optional<bool> eqOn;
    QVector<EqBand> eqBands;
    std::optional<bool> dynOn;
    std::optional<double> dynThreshold; // dB
    std::optional<double> dynRatio;
    std::optional<double> dynAttack;  // ms
    std::optional<double> dynRelease; // ms
    std::optional<double> dynGain;    // makeup dB

    [[nodiscard]] bool isEmpty() const;

    QJsonObject toJson() const;
    [[nodiscard]] static VoiceData fromJson(const QJsonObject& json);
};

// An actor's stored voice for one profile slot: the main voice plus a backup copy
// (spare-mic safe set).
class ActorProfile {
  public:
    [[nodiscard]] const VoiceData& main() const { return m_main; }
    [[nodiscard]] VoiceData& main() { return m_main; }
    void setMain(const VoiceData& voice) { m_main = voice; }

    [[nodiscard]] const VoiceData& backup() const { return m_backup; }
    [[nodiscard]] VoiceData& backup() { return m_backup; }
    void setBackup(const VoiceData& voice) { m_backup = voice; }

    [[nodiscard]] bool isEmpty() const { return m_main.isEmpty() && m_backup.isEmpty(); }

    QJsonObject toJson() const;
    [[nodiscard]] static ActorProfile fromJson(const QJsonObject& json);

  private:
    VoiceData m_main;
    VoiceData m_backup;
};

} // namespace OpenMix
