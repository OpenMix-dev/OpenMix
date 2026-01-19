#include "Cue.h"
#include <QJsonValue>

namespace StageBlend {

// cueType conversion functions
QString cueTypeToString(CueType type) {
    switch (type) {
    case CueType::Snapshot:
        return "snapshot";
    case CueType::Fade:
        return "fade";
    case CueType::Stop:
        return "stop";
    case CueType::GoTo:
        return "goto";
    case CueType::Wait:
        return "wait";
    case CueType::Macro:
        return "macro";
    default:
        return "snapshot";
    }
}

CueType stringToCueType(const QString& str) {
    if (str == "fade")
        return CueType::Fade;
    if (str == "stop")
        return CueType::Stop;
    if (str == "goto")
        return CueType::GoTo;
    if (str == "wait")
        return CueType::Wait;
    if (str == "macro")
        return CueType::Macro;
    return CueType::Snapshot;
}

// fadeCurve conversion functions
QString fadeCurveToString(FadeCurve curve) {
    switch (curve) {
    case FadeCurve::Linear:
        return "linear";
    case FadeCurve::EaseIn:
        return "easein";
    case FadeCurve::EaseOut:
        return "easeout";
    case FadeCurve::SCurve:
        return "scurve";
    case FadeCurve::Exponential:
        return "exponential";
    default:
        return "linear";
    }
}

FadeCurve stringToFadeCurve(const QString& str) {
    if (str == "easein")
        return FadeCurve::EaseIn;
    if (str == "easeout")
        return FadeCurve::EaseOut;
    if (str == "scurve")
        return FadeCurve::SCurve;
    if (str == "exponential")
        return FadeCurve::Exponential;
    return FadeCurve::Linear;
}

// autoFollowCondition conversion functions
QString autoFollowConditionToString(AutoFollowCondition condition) {
    switch (condition) {
    case AutoFollowCondition::Always:
        return "always";
    case AutoFollowCondition::AfterFadeComplete:
        return "afterfade";
    case AutoFollowCondition::OnButtonPress:
        return "onbutton";
    default:
        return "always";
    }
}

AutoFollowCondition stringToAutoFollowCondition(const QString& str) {
    if (str == "afterfade")
        return AutoFollowCondition::AfterFadeComplete;
    if (str == "onbutton")
        return AutoFollowCondition::OnButtonPress;
    return AutoFollowCondition::Always;
}

// macroExecutionMode conversion functions
QString macroExecutionModeToString(MacroExecutionMode mode) {
    switch (mode) {
    case MacroExecutionMode::Sequential:
        return "sequential";
    case MacroExecutionMode::Parallel:
        return "parallel";
    default:
        return "sequential";
    }
}

MacroExecutionMode stringToMacroExecutionMode(const QString& str) {
    if (str == "parallel")
        return MacroExecutionMode::Parallel;
    return MacroExecutionMode::Sequential;
}

Cue::Cue()
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_number(0.0),
      m_type(CueType::Snapshot), m_fadeTime(0.0), m_fadeCurve(FadeCurve::Linear),
      m_autoFollow(false), m_autoFollowDelay(0.0),
      m_autoFollowCondition(AutoFollowCondition::Always),
      m_macroExecutionMode(MacroExecutionMode::Sequential) {}

Cue::Cue(double number, const QString& name)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_number(number), m_name(name),
      m_type(CueType::Snapshot), m_fadeTime(0.0), m_fadeCurve(FadeCurve::Linear),
      m_autoFollow(false), m_autoFollowDelay(0.0),
      m_autoFollowCondition(AutoFollowCondition::Always),
      m_macroExecutionMode(MacroExecutionMode::Sequential) {}

void Cue::setParameter(const QString& path, const QVariant& value) {
    m_parameters[path] = QJsonValue::fromVariant(value);
}

QVariant Cue::parameter(const QString& path) const {
    if (m_parameters.contains(path)) {
        return m_parameters[path].toVariant();
    }
    return QVariant();
}

QJsonObject Cue::toJson() const {
    QJsonObject json;
    json["id"] = m_id;
    json["number"] = m_number;
    json["name"] = m_name;
    json["notes"] = m_notes;
    json["type"] = cueTypeToString(m_type);
    json["fadeTime"] = m_fadeTime;
    json["fadeCurve"] = fadeCurveToString(m_fadeCurve);
    json["autoFollow"] = m_autoFollow;
    json["autoFollowDelay"] = m_autoFollowDelay;
    json["autoFollowCondition"] = autoFollowConditionToString(m_autoFollowCondition);
    json["parameters"] = m_parameters;

    // macro support
    if (!m_childCueIds.isEmpty()) {
        QJsonArray childIds;
        for (const QString& id : m_childCueIds) {
            childIds.append(id);
        }
        json["childCueIds"] = childIds;
    }
    json["macroExecutionMode"] = macroExecutionModeToString(m_macroExecutionMode);

    // grouping & tags
    if (!m_group.isEmpty()) {
        json["group"] = m_group;
    }
    if (!m_tags.isEmpty()) {
        QJsonArray tagsArray;
        for (const QString& tag : m_tags) {
            tagsArray.append(tag);
        }
        json["tags"] = tagsArray;
    }

    return json;
}

Cue Cue::fromJson(const QJsonObject& json) {
    Cue cue;
    cue.m_id = json["id"].toString();
    if (cue.m_id.isEmpty()) {
        cue.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    cue.m_number = json["number"].toDouble();
    cue.m_name = json["name"].toString();
    cue.m_notes = json["notes"].toString();
    cue.m_type = stringToCueType(json["type"].toString());
    cue.m_fadeTime = json["fadeTime"].toDouble();
    cue.m_fadeCurve = stringToFadeCurve(json["fadeCurve"].toString());
    cue.m_autoFollow = json["autoFollow"].toBool();
    cue.m_autoFollowDelay = json["autoFollowDelay"].toDouble();
    cue.m_autoFollowCondition = stringToAutoFollowCondition(json["autoFollowCondition"].toString());
    cue.m_parameters = json["parameters"].toObject();

    // macro support
    if (json.contains("childCueIds")) {
        QJsonArray childIds = json["childCueIds"].toArray();
        for (const QJsonValue& val : childIds) {
            cue.m_childCueIds.append(val.toString());
        }
    }
    cue.m_macroExecutionMode = stringToMacroExecutionMode(json["macroExecutionMode"].toString());

    // grouping & tags
    cue.m_group = json["group"].toString();
    if (json.contains("tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        for (const QJsonValue& val : tagsArray) {
            cue.m_tags.append(val.toString());
        }
    }

    return cue;
}

} // namespace StageBlend
