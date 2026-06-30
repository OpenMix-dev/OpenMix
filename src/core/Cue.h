#pragma once

#include "FadeCurve.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <optional>

namespace OpenMix {

enum class CueType { Snapshot, Stop, GoTo, Wait, Macro };

enum class AutoFollowCondition {
    Always,       // always trigger after delay (default)
    OnButtonPress // require GO command (sets flag only)
};

struct DCAOverride {
    std::optional<bool> mute;     // mute state override (nullopt = don't change)
    std::optional<QString> label; // label override (nullopt = don't change)

    [[nodiscard]] bool hasOverrides() const noexcept { return mute.has_value() || label.has_value(); }

    QJsonObject toJson() const;
    [[nodiscard]] static DCAOverride fromJson(const QJsonObject& json);
};

enum class MacroExecutionMode {
    Sequential, // execute child cues one after another
    Parallel    // execute all child cues simultaneously
};

enum class StopBehavior {
    StopOnly,    // just stop, don't change mixer state
    StopAndApply // stop & immediately apply cue parameters
};

QString cueTypeToString(CueType type);
CueType stringToCueType(const QString& str);
QString autoFollowConditionToString(AutoFollowCondition condition);
AutoFollowCondition stringToAutoFollowCondition(const QString& str);
QString macroExecutionModeToString(MacroExecutionMode mode);
MacroExecutionMode stringToMacroExecutionMode(const QString& str);
QString stopBehaviorToString(StopBehavior behavior);
StopBehavior stringToStopBehavior(const QString& str);

class Cue {
  public:
    Cue();
    explicit Cue(double number, const QString& name = QString());

    [[nodiscard]] QString id() const { return m_id; }
    void regenerateId();

    [[nodiscard]] double number() const noexcept { return m_number; }
    void setNumber(double number) { m_number = number; }

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    [[nodiscard]] QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; }

    [[nodiscard]] CueType type() const noexcept { return m_type; }
    void setType(CueType type) { m_type = type; }

    [[nodiscard]] bool autoFollow() const noexcept { return m_autoFollow; }
    void setAutoFollow(bool autoFollow) { m_autoFollow = autoFollow; }

    [[nodiscard]] double autoFollowDelay() const noexcept { return m_autoFollowDelay; }
    void setAutoFollowDelay(double seconds) { m_autoFollowDelay = seconds; }

    [[nodiscard]] AutoFollowCondition autoFollowCondition() const noexcept {
        return m_autoFollowCondition;
    }
    void setAutoFollowCondition(AutoFollowCondition condition) {
        m_autoFollowCondition = condition;
    }

    // fade transition applied to fader/level moves on fire (0 = instant)
    [[nodiscard]] double fadeTime() const noexcept { return m_fadeTime; }
    void setFadeTime(double seconds) { m_fadeTime = seconds; }
    [[nodiscard]] FadeCurve fadeCurve() const noexcept { return m_fadeCurve; }
    void setFadeCurve(FadeCurve curve) { m_fadeCurve = curve; }

    // DCA targeting
    [[nodiscard]] QSet<int> targetedDCAs() const { return m_targetedDCAs; }
    void setTargetedDCAs(const QSet<int>& dcas) { m_targetedDCAs = dcas; }
    void addTargetedDCA(int dca) { m_targetedDCAs.insert(dca); }
    void removeTargetedDCA(int dca) { m_targetedDCAs.remove(dca); }
    void clearTargetedDCAs() { m_targetedDCAs.clear(); }
    [[nodiscard]] bool targetsAllDCAs() const noexcept { return m_targetedDCAs.isEmpty(); }
    [[nodiscard]] bool targetsDCA(int dca) const {
        return m_targetedDCAs.isEmpty() || m_targetedDCAs.contains(dca);
    }

    // per-DCA overrides
    [[nodiscard]] QMap<int, DCAOverride> dcaOverrides() const { return m_dcaOverrides; }
    void setDCAOverrides(const QMap<int, DCAOverride>& overrides) { m_dcaOverrides = overrides; }
    void setDCAOverride(int dca, const DCAOverride& override);
    [[nodiscard]] DCAOverride dcaOverride(int dca) const;
    void clearDCAOverride(int dca);
    void clearAllDCAOverrides() { m_dcaOverrides.clear(); }

    // per-cue DCA mapping (overrides show-level mapping during playback)
    [[nodiscard]] bool hasCustomDCAMapping() const;
    void setDCAChannelMapping(const QMap<int, QList<int>>& mapping);
    void setDCABusMapping(const QMap<int, QList<int>>& mapping);
    [[nodiscard]] QMap<int, QList<int>> dcaChannelMapping() const;
    [[nodiscard]] QMap<int, QList<int>> dcaBusMapping() const;
    void clearCustomDCAMapping();
    void copyDCAMappingFrom(const class DCAMapping* showMapping);

    [[nodiscard]] bool isMacro() const noexcept { return m_type == CueType::Macro; }
    [[nodiscard]] QStringList childCueIds() const { return m_childCueIds; }
    void setChildCueIds(const QStringList& ids) { m_childCueIds = ids; }
    void addChildCueId(const QString& id) { m_childCueIds.append(id); }
    void removeChildCueId(const QString& id) { m_childCueIds.removeAll(id); }
    void clearChildCueIds() { m_childCueIds.clear(); }

    [[nodiscard]] MacroExecutionMode macroExecutionMode() const noexcept {
        return m_macroExecutionMode;
    }
    void setMacroExecutionMode(MacroExecutionMode mode) { m_macroExecutionMode = mode; }

    [[nodiscard]] QString gotoTarget() const { return m_gotoTarget; }
    void setGotoTarget(const QString& target) { m_gotoTarget = target; }

    [[nodiscard]] bool gotoAutoExecute() const noexcept { return m_gotoAutoExecute; }
    void setGotoAutoExecute(bool autoExec) { m_gotoAutoExecute = autoExec; }

    [[nodiscard]] StopBehavior stopBehavior() const noexcept { return m_stopBehavior; }
    void setStopBehavior(StopBehavior behavior) { m_stopBehavior = behavior; }

    [[nodiscard]] QString group() const { return m_group; }
    void setGroup(const QString& group) { m_group = group; }

    // linked QLab cue id fired on execute (outbound DAW remote)
    [[nodiscard]] QString qLabCue() const { return m_qLabCue; }
    void setQLabCue(const QString& cueId) { m_qLabCue = cueId; }

    [[nodiscard]] QStringList tags() const { return m_tags; }
    void setTags(const QStringList& tags) { m_tags = tags; }
    void addTag(const QString& tag) {
        if (!m_tags.contains(tag))
            m_tags.append(tag);
    }
    void removeTag(const QString& tag) { m_tags.removeAll(tag); }

    [[nodiscard]] QJsonObject parameters() const { return m_parameters; }
    void setParameters(const QJsonObject& params) { m_parameters = params; }
    void setParameter(const QString& path, const QVariant& value);
    [[nodiscard]] QVariant parameter(const QString& path) const;
    void clearParameters() { m_parameters = QJsonObject(); }

    // per-channel active actor-profile slot (channel -> slot id). On fire, the
    // active actor on each channel has its stored voice for that slot applied.
    [[nodiscard]] QMap<int, QString> channelProfiles() const { return m_channelProfiles; }
    void setChannelProfiles(const QMap<int, QString>& profiles) { m_channelProfiles = profiles; }
    void setChannelProfile(int channel, const QString& slot) { m_channelProfiles[channel] = slot; }
    void removeChannelProfile(int channel) { m_channelProfiles.remove(channel); }

    // per-channel fader level override (channel -> 0..1)
    [[nodiscard]] QMap<int, double> channelLevels() const { return m_channelLevels; }
    void setChannelLevels(const QMap<int, double>& levels) { m_channelLevels = levels; }
    void setChannelLevel(int channel, double level) { m_channelLevels[channel] = level; }
    void removeChannelLevel(int channel) { m_channelLevels.remove(channel); }

    QJsonObject toJson() const;
    [[nodiscard]] static Cue fromJson(const QJsonObject& json);

    bool operator<(const Cue& other) const noexcept { return m_number < other.m_number; }
    bool operator==(const Cue& other) const noexcept { return m_id == other.m_id; }

  private:
    QString m_id;
    double m_number;
    QString m_name;
    QString m_notes;
    CueType m_type;

    bool m_autoFollow;
    double m_autoFollowDelay;
    AutoFollowCondition m_autoFollowCondition;

    double m_fadeTime = 0.0; // fade duration (seconds); 0 = instant
    FadeCurve m_fadeCurve = FadeCurve::EaseInOut;

    // DCA targeting
    QSet<int> m_targetedDCAs;              // empty = all DCAs
    QMap<int, DCAOverride> m_dcaOverrides; // per-DCA mute/label overrides

    // per-cue DCA mapping (overrides show-level mapping during playback)
    std::optional<QMap<int, QList<int>>> m_dcaChannelMapping; // DCA# -> [channel#, ...]
    std::optional<QMap<int, QList<int>>> m_dcaBusMapping;     // DCA# -> [bus#, ...]

    QStringList m_childCueIds;
    MacroExecutionMode m_macroExecutionMode;

    QString m_gotoTarget;
    bool m_gotoAutoExecute = false;

    StopBehavior m_stopBehavior = StopBehavior::StopAndApply;

    QString m_group;
    QStringList m_tags;
    QString m_qLabCue; // linked QLab cue id (DAW remote)

    QJsonObject m_parameters;

    QMap<int, QString> m_channelProfiles; // channel -> active profile slot id
    QMap<int, double> m_channelLevels;    // channel -> fader level override (0..1)
};

} // namespace OpenMix
