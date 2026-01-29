#pragma once

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

    bool hasOverrides() const { return mute.has_value() || label.has_value(); }

    QJsonObject toJson() const;
    static DCAOverride fromJson(const QJsonObject& json);
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

    QString id() const { return m_id; }
    void regenerateId();

    double number() const { return m_number; }
    void setNumber(double number) { m_number = number; }

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; }

    CueType type() const { return m_type; }
    void setType(CueType type) { m_type = type; }

    bool autoFollow() const { return m_autoFollow; }
    void setAutoFollow(bool autoFollow) { m_autoFollow = autoFollow; }

    double autoFollowDelay() const { return m_autoFollowDelay; }
    void setAutoFollowDelay(double seconds) { m_autoFollowDelay = seconds; }

    AutoFollowCondition autoFollowCondition() const { return m_autoFollowCondition; }
    void setAutoFollowCondition(AutoFollowCondition condition) {
        m_autoFollowCondition = condition;
    }

    // DCA targeting
    QSet<int> targetedDCAs() const { return m_targetedDCAs; }
    void setTargetedDCAs(const QSet<int>& dcas) { m_targetedDCAs = dcas; }
    void addTargetedDCA(int dca) { m_targetedDCAs.insert(dca); }
    void removeTargetedDCA(int dca) { m_targetedDCAs.remove(dca); }
    void clearTargetedDCAs() { m_targetedDCAs.clear(); }
    bool targetsAllDCAs() const { return m_targetedDCAs.isEmpty(); }
    bool targetsDCA(int dca) const {
        return m_targetedDCAs.isEmpty() || m_targetedDCAs.contains(dca);
    }

    // per-DCA overrides
    QMap<int, DCAOverride> dcaOverrides() const { return m_dcaOverrides; }
    void setDCAOverrides(const QMap<int, DCAOverride>& overrides) { m_dcaOverrides = overrides; }
    void setDCAOverride(int dca, const DCAOverride& override);
    DCAOverride dcaOverride(int dca) const;
    void clearDCAOverride(int dca);
    void clearAllDCAOverrides() { m_dcaOverrides.clear(); }

    bool isMacro() const { return m_type == CueType::Macro; }
    QStringList childCueIds() const { return m_childCueIds; }
    void setChildCueIds(const QStringList& ids) { m_childCueIds = ids; }
    void addChildCueId(const QString& id) { m_childCueIds.append(id); }
    void removeChildCueId(const QString& id) { m_childCueIds.removeAll(id); }
    void clearChildCueIds() { m_childCueIds.clear(); }

    MacroExecutionMode macroExecutionMode() const { return m_macroExecutionMode; }
    void setMacroExecutionMode(MacroExecutionMode mode) { m_macroExecutionMode = mode; }

    QString gotoTarget() const { return m_gotoTarget; }
    void setGotoTarget(const QString& target) { m_gotoTarget = target; }

    bool gotoAutoExecute() const { return m_gotoAutoExecute; }
    void setGotoAutoExecute(bool autoExec) { m_gotoAutoExecute = autoExec; }

    StopBehavior stopBehavior() const { return m_stopBehavior; }
    void setStopBehavior(StopBehavior behavior) { m_stopBehavior = behavior; }

    QString group() const { return m_group; }
    void setGroup(const QString& group) { m_group = group; }

    QStringList tags() const { return m_tags; }
    void setTags(const QStringList& tags) { m_tags = tags; }
    void addTag(const QString& tag) {
        if (!m_tags.contains(tag))
            m_tags.append(tag);
    }
    void removeTag(const QString& tag) { m_tags.removeAll(tag); }

    QJsonObject parameters() const { return m_parameters; }
    void setParameters(const QJsonObject& params) { m_parameters = params; }
    void setParameter(const QString& path, const QVariant& value);
    QVariant parameter(const QString& path) const;
    void clearParameters() { m_parameters = QJsonObject(); }

    QJsonObject toJson() const;
    static Cue fromJson(const QJsonObject& json);

    bool operator<(const Cue& other) const { return m_number < other.m_number; }
    bool operator==(const Cue& other) const { return m_id == other.m_id; }

  private:
    QString m_id;
    double m_number;
    QString m_name;
    QString m_notes;
    CueType m_type;

    bool m_autoFollow;
    double m_autoFollowDelay;
    AutoFollowCondition m_autoFollowCondition;

    // DCA targeting
    QSet<int> m_targetedDCAs;              // empty = all DCAs
    QMap<int, DCAOverride> m_dcaOverrides; // per-DCA mute/label overrides

    QStringList m_childCueIds;
    MacroExecutionMode m_macroExecutionMode;

    QString m_gotoTarget;
    bool m_gotoAutoExecute = false;

    StopBehavior m_stopBehavior = StopBehavior::StopAndApply;

    QString m_group;
    QStringList m_tags;

    QJsonObject m_parameters;
};

} // namespace OpenMix
