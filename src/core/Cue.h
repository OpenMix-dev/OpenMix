#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace OpenMix {

enum class CueType {
    Snapshot, // instant recall of all parameters
    Fade,     // timed crossfade to target values
    Stop,     // stop/mute action
    GoTo,     // jump to another cue
    Wait,     // timed pause in auto-follow sequence
    Macro     // execute multiple child cues
};

enum class FadeCurve {
    Linear,     // linear interpolation (default)
    EaseIn,     // slow start, fast end (quadratic)
    EaseOut,    // fast start, slow end (quadratic)
    SCurve,     // smooth ease in & out
    Exponential // exponential curve
};

enum class AutoFollowCondition {
    Always,            // always trigger after delay (default)
    AfterFadeComplete, // wait for fade to complete, then apply delay
    OnButtonPress      // require explicit GO command (sets flag only)
};

enum class MacroExecutionMode {
    Sequential, // execute child cues one after another
    Parallel    // execute all child cues simultaneously
};

enum class StopBehavior {
    StopFadesOnly, // just stop active fades, don't change mixer state
    StopAndApply,  // stop fades & immediately apply cue parameters
    StopAndFadeOut // stop fades & fade to cue parameters using fadeTime
};

QString cueTypeToString(CueType type);
CueType stringToCueType(const QString& str);
QString fadeCurveToString(FadeCurve curve);
FadeCurve stringToFadeCurve(const QString& str);
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
    double number() const { return m_number; }
    void setNumber(double number) { m_number = number; }

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; }

    CueType type() const { return m_type; }
    void setType(CueType type) { m_type = type; }

    double fadeTime() const { return m_fadeTime; }
    void setFadeTime(double seconds) { m_fadeTime = seconds; }

    FadeCurve fadeCurve() const { return m_fadeCurve; }
    void setFadeCurve(FadeCurve curve) { m_fadeCurve = curve; }

    bool autoFollow() const { return m_autoFollow; }
    void setAutoFollow(bool autoFollow) { m_autoFollow = autoFollow; }

    double autoFollowDelay() const { return m_autoFollowDelay; }
    void setAutoFollowDelay(double seconds) { m_autoFollowDelay = seconds; }

    AutoFollowCondition autoFollowCondition() const { return m_autoFollowCondition; }
    void setAutoFollowCondition(AutoFollowCondition condition) {
        m_autoFollowCondition = condition;
    }

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

    double m_fadeTime;
    FadeCurve m_fadeCurve;

    bool m_autoFollow;
    double m_autoFollowDelay;
    AutoFollowCondition m_autoFollowCondition;

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
