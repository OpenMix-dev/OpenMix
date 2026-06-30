#include "Cue.h"
#include "DCAMapping.h"
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

bool Cue::hasCustomDCAMapping() const {
    return m_dcaChannelMapping.has_value() || m_dcaBusMapping.has_value();
}

void Cue::setDCAChannelMapping(const QMap<int, QList<int>>& mapping) {
    m_dcaChannelMapping = mapping;
}

void Cue::setDCABusMapping(const QMap<int, QList<int>>& mapping) { m_dcaBusMapping = mapping; }

QMap<int, QList<int>> Cue::dcaChannelMapping() const {
    return m_dcaChannelMapping.value_or(QMap<int, QList<int>>());
}

QMap<int, QList<int>> Cue::dcaBusMapping() const {
    return m_dcaBusMapping.value_or(QMap<int, QList<int>>());
}

void Cue::clearCustomDCAMapping() {
    m_dcaChannelMapping.reset();
    m_dcaBusMapping.reset();
}

void Cue::copyDCAMappingFrom(const DCAMapping* showMapping) {
    if (showMapping) {
        m_dcaChannelMapping = showMapping->channelAssignments();
        m_dcaBusMapping = showMapping->busAssignments();
    }
}

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
    if (m_fadeTime > 0.0) {
        json["fadeTime"] = m_fadeTime;
        json["fadeCurve"] = fadeCurveToString(m_fadeCurve);
    }
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

    // per-cue DCA mapping
    if (hasCustomDCAMapping()) {
        QJsonObject dcaMappingObj;
        if (m_dcaChannelMapping.has_value()) {
            QJsonObject channelsObj;
            for (auto it = m_dcaChannelMapping->constBegin(); it != m_dcaChannelMapping->constEnd();
                 ++it) {
                QJsonArray arr;
                for (int ch : it.value()) {
                    arr.append(ch);
                }
                channelsObj[QString::number(it.key())] = arr;
            }
            dcaMappingObj["channels"] = channelsObj;
        }
        if (m_dcaBusMapping.has_value()) {
            QJsonObject busesObj;
            for (auto it = m_dcaBusMapping->constBegin(); it != m_dcaBusMapping->constEnd(); ++it) {
                QJsonArray arr;
                for (int bus : it.value()) {
                    arr.append(bus);
                }
                busesObj[QString::number(it.key())] = arr;
            }
            dcaMappingObj["buses"] = busesObj;
        }
        json["dcaMapping"] = dcaMappingObj;
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
    if (!m_qLabCue.isEmpty()) {
        json["qLabCue"] = m_qLabCue;
    }
    if (!m_tags.isEmpty()) {
        QJsonArray tagsArray;
        for (const QString& tag : m_tags) {
            tagsArray.append(tag);
        }
        json["tags"] = tagsArray;
    }

    // per-channel actor-profile slots: { "<channel>": "<slot>" }
    if (!m_channelProfiles.isEmpty()) {
        QJsonObject profilesObj;
        for (auto it = m_channelProfiles.constBegin(); it != m_channelProfiles.constEnd(); ++it) {
            profilesObj[QString::number(it.key())] = it.value();
        }
        json["channelProfiles"] = profilesObj;
    }

    // per-channel level overrides: { "<channel>": level }
    if (!m_channelLevels.isEmpty()) {
        QJsonObject levelsObj;
        for (auto it = m_channelLevels.constBegin(); it != m_channelLevels.constEnd(); ++it) {
            levelsObj[QString::number(it.key())] = it.value();
        }
        json["channelLevels"] = levelsObj;
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
    cue.m_fadeTime = json["fadeTime"].toDouble(0.0);
    cue.m_fadeCurve = stringToFadeCurve(json["fadeCurve"].toString());
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

    // per-cue DCA mapping
    if (json.contains("dcaMapping")) {
        QJsonObject dcaMappingObj = json["dcaMapping"].toObject();
        if (dcaMappingObj.contains("channels")) {
            QMap<int, QList<int>> channelMapping;
            QJsonObject channelsObj = dcaMappingObj["channels"].toObject();
            for (auto it = channelsObj.constBegin(); it != channelsObj.constEnd(); ++it) {
                int dca = it.key().toInt();
                QList<int> channels;
                for (const QJsonValue& val : it.value().toArray()) {
                    channels.append(val.toInt());
                }
                channelMapping[dca] = channels;
            }
            cue.m_dcaChannelMapping = channelMapping;
        }

        if (dcaMappingObj.contains("buses")) {
            QMap<int, QList<int>> busMapping;
            QJsonObject busesObj = dcaMappingObj["buses"].toObject();
            for (auto it = busesObj.constBegin(); it != busesObj.constEnd(); ++it) {
                int dca = it.key().toInt();
                QList<int> buses;
                for (const QJsonValue& val : it.value().toArray()) {
                    buses.append(val.toInt());
                }
                busMapping[dca] = buses;
            }
            cue.m_dcaBusMapping = busMapping;
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
    cue.m_qLabCue = json["qLabCue"].toString();
    if (json.contains("tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        for (const QJsonValue& val : tagsArray) {
            cue.m_tags.append(val.toString());
        }
    }

    // per-channel actor-profile slots
    if (json.contains("channelProfiles")) {
        const QJsonObject profilesObj = json["channelProfiles"].toObject();
        for (auto it = profilesObj.constBegin(); it != profilesObj.constEnd(); ++it) {
            cue.m_channelProfiles[it.key().toInt()] = it.value().toString();
        }
    }

    // per-channel level overrides
    if (json.contains("channelLevels")) {
        const QJsonObject levelsObj = json["channelLevels"].toObject();
        for (auto it = levelsObj.constBegin(); it != levelsObj.constEnd(); ++it) {
            cue.m_channelLevels[it.key().toInt()] = it.value().toDouble();
        }
    }

    return cue;
}

} // namespace OpenMix
