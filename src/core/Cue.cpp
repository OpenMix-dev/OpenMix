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

void Cue::setChannelPosition(int channel, const QString& positionId) {
    if (positionId.isEmpty()) {
        m_channelPositions.remove(channel);
    } else {
        m_channelPositions[channel] = positionId;
    }
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

void Cue::assignChannelToDCAMapping(int channel, int dca, const DCAMapping* seedFrom) {
    if (!hasCustomDCAMapping() && seedFrom)
        copyDCAMappingFrom(seedFrom);

    QMap<int, QList<int>> channelMap = dcaChannelMapping();
    for (QList<int>& channels : channelMap)
        channels.removeAll(channel);
    channelMap[dca].append(channel);
    setDCAChannelMapping(channelMap);
}

void Cue::assignChannelsToDCAMapping(const QList<int>& channels, int dca,
                                     const DCAMapping* seedFrom) {
    if (!hasCustomDCAMapping() && seedFrom)
        copyDCAMappingFrom(seedFrom);

    QMap<int, QList<int>> channelMap = dcaChannelMapping();
    for (QList<int>& list : channelMap) // one DCA per channel
        for (int ch : channels)
            list.removeAll(ch);
    channelMap[dca] = channels; // the label declares the DCA's membership
    setDCAChannelMapping(channelMap);
}

void Cue::mergeContentFrom(const Cue& other) {
    // scalar content overwritten from the other cue
    m_type = other.m_type;
    m_autoFollow = other.m_autoFollow;
    m_autoFollowDelay = other.m_autoFollowDelay;
    m_autoFollowCondition = other.m_autoFollowCondition;
    m_fadeTime = other.m_fadeTime;
    m_fadeCurve = other.m_fadeCurve;
    m_macroExecutionMode = other.m_macroExecutionMode;
    m_gotoTarget = other.m_gotoTarget;
    m_gotoAutoExecute = other.m_gotoAutoExecute;
    m_stopBehavior = other.m_stopBehavior;
    m_group = other.m_group;
    m_qLabCue = other.m_qLabCue;
    m_color = other.m_color;
    m_skip = other.m_skip;

    // set/map content unites, other winning on collisions
    m_targetedDCAs.unite(other.m_targetedDCAs);
    for (auto it = other.m_dcaOverrides.begin(); it != other.m_dcaOverrides.end(); ++it)
        m_dcaOverrides.insert(it.key(), it.value());
    for (auto it = other.m_channelPositions.begin(); it != other.m_channelPositions.end(); ++it)
        m_channelPositions.insert(it.key(), it.value());
    for (auto it = other.m_channelProfiles.begin(); it != other.m_channelProfiles.end(); ++it)
        m_channelProfiles.insert(it.key(), it.value());
    for (auto it = other.m_channelLevels.begin(); it != other.m_channelLevels.end(); ++it)
        m_channelLevels.insert(it.key(), it.value());
    for (auto it = other.m_fxMutes.begin(); it != other.m_fxMutes.end(); ++it)
        m_fxMutes.insert(it.key(), it.value());
    if (other.m_dcaChannelMapping) {
        QMap<int, QList<int>> merged = m_dcaChannelMapping.value_or(QMap<int, QList<int>>());
        for (auto it = other.m_dcaChannelMapping->begin(); it != other.m_dcaChannelMapping->end();
             ++it)
            merged.insert(it.key(), it.value());
        m_dcaChannelMapping = merged;
    }
    if (other.m_dcaBusMapping) {
        QMap<int, QList<int>> merged = m_dcaBusMapping.value_or(QMap<int, QList<int>>());
        for (auto it = other.m_dcaBusMapping->begin(); it != other.m_dcaBusMapping->end(); ++it)
            merged.insert(it.key(), it.value());
        m_dcaBusMapping = merged;
    }

    // list content unites without duplicating
    for (const QString& id : other.m_childCueIds)
        if (!m_childCueIds.contains(id))
            m_childCueIds.append(id);
    for (const QString& tag : other.m_tags)
        if (!m_tags.contains(tag))
            m_tags.append(tag);
    for (int snippet : other.m_snippets)
        if (!m_snippets.contains(snippet))
            m_snippets.append(snippet);
    for (int scene : other.m_scenes)
        if (!m_scenes.contains(scene))
            m_scenes.append(scene);
    for (auto it = other.m_channelFX.begin(); it != other.m_channelFX.end(); ++it)
        m_channelFX.insert(it.key(), it.value());

    // parameter bag: other's keys overlay
    for (auto it = other.m_parameters.begin(); it != other.m_parameters.end(); ++it)
        m_parameters.insert(it.key(), it.value());
}

void Cue::swapContentWith(Cue& other) {
    std::swap(m_type, other.m_type);
    std::swap(m_autoFollow, other.m_autoFollow);
    std::swap(m_autoFollowDelay, other.m_autoFollowDelay);
    std::swap(m_autoFollowCondition, other.m_autoFollowCondition);
    std::swap(m_fadeTime, other.m_fadeTime);
    std::swap(m_fadeCurve, other.m_fadeCurve);
    std::swap(m_targetedDCAs, other.m_targetedDCAs);
    std::swap(m_dcaOverrides, other.m_dcaOverrides);
    std::swap(m_dcaChannelMapping, other.m_dcaChannelMapping);
    std::swap(m_dcaBusMapping, other.m_dcaBusMapping);
    std::swap(m_childCueIds, other.m_childCueIds);
    std::swap(m_macroExecutionMode, other.m_macroExecutionMode);
    std::swap(m_gotoTarget, other.m_gotoTarget);
    std::swap(m_gotoAutoExecute, other.m_gotoAutoExecute);
    std::swap(m_stopBehavior, other.m_stopBehavior);
    std::swap(m_group, other.m_group);
    std::swap(m_tags, other.m_tags);
    std::swap(m_qLabCue, other.m_qLabCue);
    std::swap(m_channelPositions, other.m_channelPositions);
    std::swap(m_parameters, other.m_parameters);
    std::swap(m_channelProfiles, other.m_channelProfiles);
    std::swap(m_channelLevels, other.m_channelLevels);
    std::swap(m_fxMutes, other.m_fxMutes);
    std::swap(m_snippets, other.m_snippets);
    std::swap(m_scenes, other.m_scenes);
    std::swap(m_channelFX, other.m_channelFX);
    std::swap(m_color, other.m_color);
    std::swap(m_skip, other.m_skip);
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

    // named-position assignments
    if (!m_channelPositions.isEmpty()) {
        QJsonObject positionsObj;
        for (auto it = m_channelPositions.constBegin(); it != m_channelPositions.constEnd(); ++it) {
            positionsObj[QString::number(it.key())] = it.value();
        }
        json["channelPositions"] = positionsObj;
    }

    // per-FX-unit mute states: { "<unit>": muted }
    if (!m_fxMutes.isEmpty()) {
        QJsonObject fxObj;
        for (auto it = m_fxMutes.constBegin(); it != m_fxMutes.constEnd(); ++it) {
            fxObj[QString::number(it.key())] = it.value();
        }
        json["fxMutes"] = fxObj;
    }

    // console snippets recalled on fire
    if (!m_snippets.isEmpty()) {
        QJsonArray snippetArray;
        for (int snippet : m_snippets) {
            snippetArray.append(snippet);
        }
        json["snippets"] = snippetArray;
    }

    if (!m_scenes.isEmpty()) {
        QJsonArray sceneArray;
        for (int scene : m_scenes)
            sceneArray.append(scene);
        json["scenes"] = sceneArray;
    }

    if (!m_channelFX.isEmpty()) {
        QJsonObject fxChanObj;
        for (auto it = m_channelFX.constBegin(); it != m_channelFX.constEnd(); ++it)
            fxChanObj[QString::number(it.key())] = it.value();
        json["channelFX"] = fxChanObj;
    }

    if (!m_color.isEmpty()) {
        json["color"] = m_color;
    }
    json["skip"] = m_skip;

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

    // named-position assignments
    if (json.contains("channelPositions")) {
        QJsonObject positionsObj = json["channelPositions"].toObject();
        for (auto it = positionsObj.constBegin(); it != positionsObj.constEnd(); ++it) {
            cue.m_channelPositions[it.key().toInt()] = it.value().toString();
        }
    }

    // per-FX-unit mute states
    if (json.contains("fxMutes")) {
        const QJsonObject fxObj = json["fxMutes"].toObject();
        for (auto it = fxObj.constBegin(); it != fxObj.constEnd(); ++it) {
            cue.m_fxMutes[it.key().toInt()] = it.value().toBool();
        }
    }

    // console snippets
    if (json.contains("snippets")) {
        const QJsonArray snippetArray = json["snippets"].toArray();
        for (const QJsonValue& val : snippetArray) {
            cue.m_snippets.append(val.toInt());
        }
    }

    if (json.contains("scenes")) {
        const QJsonArray sceneArray = json["scenes"].toArray();
        for (const QJsonValue& val : sceneArray)
            cue.m_scenes.append(val.toInt());
    }

    if (json.contains("channelFX")) {
        const QJsonObject fxChanObj = json["channelFX"].toObject();
        for (auto it = fxChanObj.constBegin(); it != fxChanObj.constEnd(); ++it)
            cue.m_channelFX[it.key().toInt()] = it.value().toBool();
    }

    cue.m_color = json["color"].toString();
    cue.m_skip = json["skip"].toBool(false);

    return cue;
}

} // namespace OpenMix
