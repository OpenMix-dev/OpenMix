#include "Cue.h"
#include <QJsonValue>

namespace OpenMix {

QString cueTypeToString(CueType type) {
    switch (type) {
    case CueType::Snapshot:
        return "snapshot";
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
    if (str == "snapshot")
        return CueType::Snapshot;
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

QString autoFollowConditionToString(AutoFollowCondition condition) {
    switch (condition) {
    case AutoFollowCondition::Always:
        return "always";
    case AutoFollowCondition::OnButtonPress:
        return "onbutton";
    default:
        return "always";
    }
}

AutoFollowCondition stringToAutoFollowCondition(const QString& str) {
    if (str == "always")
        return AutoFollowCondition::Always;
    if (str == "onbutton")
        return AutoFollowCondition::OnButtonPress;
    return AutoFollowCondition::Always;
}

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

QString stopBehaviorToString(StopBehavior behavior) {
    switch (behavior) {
    case StopBehavior::StopOnly:
        return "stoponly";
    case StopBehavior::StopAndApply:
        return "stopandapply";
    default:
        return "stopandapply";
    }
}

StopBehavior stringToStopBehavior(const QString& str) {
    if (str == "stoponly")
        return StopBehavior::StopOnly;
    if (str == "stopandapply")
        return StopBehavior::StopAndApply;
    return StopBehavior::StopAndApply;
}

// DCAOverride serialization
QJsonObject DCAOverride::toJson() const {
    QJsonObject json;
    if (mute.has_value()) {
        json["mute"] = mute.value();
    }
    if (label.has_value()) {
        json["label"] = label.value();
    }
    return json;
}

DCAOverride DCAOverride::fromJson(const QJsonObject& json) {
    DCAOverride override;
    if (json.contains("mute")) {
        override.mute = json["mute"].toBool();
    }
    if (json.contains("label")) {
        override.label = json["label"].toString();
    }
    return override;
}

Cue::Cue()
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_number(0.0),
      m_type(CueType::Snapshot), m_autoFollow(false), m_autoFollowDelay(0.0),
      m_autoFollowCondition(AutoFollowCondition::Always),
      m_macroExecutionMode(MacroExecutionMode::Sequential), m_gotoAutoExecute(false) {}

Cue::Cue(double number, const QString& name)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_number(number), m_name(name),
      m_type(CueType::Snapshot), m_autoFollow(false), m_autoFollowDelay(0.0),
      m_autoFollowCondition(AutoFollowCondition::Always),
      m_macroExecutionMode(MacroExecutionMode::Sequential), m_gotoAutoExecute(false) {}

void Cue::regenerateId() { m_id = QUuid::createUuid().toString(QUuid::WithoutBraces); }

void Cue::setParameter(const QString& path, const QVariant& value) {
    m_parameters[path] = QJsonValue::fromVariant(value);
}

QVariant Cue::parameter(const QString& path) const {
    if (m_parameters.contains(path)) {
        return m_parameters[path].toVariant();
    }
    return QVariant();
}

void Cue::setDCAOverride(int dca, const DCAOverride& override) {
    if (override.hasOverrides()) {
        m_dcaOverrides[dca] = override;
    } else {
        m_dcaOverrides.remove(dca);
    }
}

DCAOverride Cue::dcaOverride(int dca) const { return m_dcaOverrides.value(dca); }

void Cue::clearDCAOverride(int dca) { m_dcaOverrides.remove(dca); }

QJsonObject Cue::toJson() const {
    QJsonObject json;
    json["id"] = m_id;
    json["number"] = m_number;
    json["name"] = m_name;
    json["notes"] = m_notes;
    json["type"] = cueTypeToString(m_type);
    json["autoFollow"] = m_autoFollow;
    json["autoFollowDelay"] = m_autoFollowDelay;
    json["autoFollowCondition"] = autoFollowConditionToString(m_autoFollowCondition);
    json["parameters"] = m_parameters;

    // DCA targeting
    if (!m_targetedDCAs.isEmpty()) {
        QJsonArray dcaArray;
        for (int dca : m_targetedDCAs) {
            dcaArray.append(dca);
        }
        json["targetedDCAs"] = dcaArray;
    }

    // DCA overrides
    if (!m_dcaOverrides.isEmpty()) {
        QJsonObject overridesObj;
        for (auto it = m_dcaOverrides.constBegin(); it != m_dcaOverrides.constEnd(); ++it) {
            overridesObj[QString::number(it.key())] = it.value().toJson();
        }
        json["dcaOverrides"] = overridesObj;
    }

    if (!m_childCueIds.isEmpty()) {
        QJsonArray childIds;
        for (const QString& id : m_childCueIds) {
            childIds.append(id);
        }
        json["childCueIds"] = childIds;
    }
    json["macroExecutionMode"] = macroExecutionModeToString(m_macroExecutionMode);

    if (!m_gotoTarget.isEmpty()) {
        json["gotoTarget"] = m_gotoTarget;
    }
    json["gotoAutoExecute"] = m_gotoAutoExecute;

    json["stopBehavior"] = stopBehaviorToString(m_stopBehavior);

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
    cue.m_autoFollow = json["autoFollow"].toBool();
    cue.m_autoFollowDelay = json["autoFollowDelay"].toDouble();
    cue.m_autoFollowCondition = stringToAutoFollowCondition(json["autoFollowCondition"].toString());
    cue.m_parameters = json["parameters"].toObject();

    // DCA targeting
    if (json.contains("targetedDCAs")) {
        QJsonArray dcaArray = json["targetedDCAs"].toArray();
        for (const QJsonValue& val : dcaArray) {
            cue.m_targetedDCAs.insert(val.toInt());
        }
    }

    // DCA overrides
    if (json.contains("dcaOverrides")) {
        QJsonObject overridesObj = json["dcaOverrides"].toObject();
        for (auto it = overridesObj.constBegin(); it != overridesObj.constEnd(); ++it) {
            int dca = it.key().toInt();
            cue.m_dcaOverrides[dca] = DCAOverride::fromJson(it.value().toObject());
        }
    }

    if (json.contains("childCueIds")) {
        QJsonArray childIds = json["childCueIds"].toArray();
        for (const QJsonValue& val : childIds) {
            cue.m_childCueIds.append(val.toString());
        }
    }
    cue.m_macroExecutionMode = stringToMacroExecutionMode(json["macroExecutionMode"].toString());

    cue.m_gotoTarget = json["gotoTarget"].toString();
    cue.m_gotoAutoExecute = json["gotoAutoExecute"].toBool(false);

    cue.m_stopBehavior = stringToStopBehavior(json["stopBehavior"].toString());

    cue.m_group = json["group"].toString();
    if (json.contains("tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        for (const QJsonValue& val : tagsArray) {
            cue.m_tags.append(val.toString());
        }
    }

    return cue;
}

} // namespace OpenMix
