#include "Show.h"
#include <QJsonArray>

namespace OpenMix {

QJsonObject MixerConfig::toJson() const {
    QJsonObject json;
    json["type"] = type;
    json["host"] = host;
    json["port"] = port;
    json["dcaCount"] = dcaCount;
    return json;
}

MixerConfig MixerConfig::fromJson(const QJsonObject& json) {
    MixerConfig config;
    config.type = json["type"].toString("x32");
    config.host = json["host"].toString();
    config.port = json["port"].toInt(10023);
    config.dcaCount = json["dcaCount"].toInt(8);
    return config;
}

Show::Show(QObject* parent)
    : QObject(parent), m_cueList(this), m_dcaMapping(this), m_actorProfileLibrary(this),
      m_positionLibrary(this), m_ensembleLibrary(this), m_cueZero(this) {
    connectCueListSignals();
    connectDcaMappingSignals();
    connectActorLibrarySignals();
    connectPositionLibrarySignals();
    connectEnsembleLibrarySignals();
    connectCueZeroSignals();
    newShow();
}

void Show::setName(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit nameChanged(name);
        setModified(true);
    }
}

bool Show::isModified() const { return m_isDirty; }

void Show::setModified(bool modified) {
    if (m_isDirty == modified)
        return;
    m_isDirty = modified;
    emit modifiedChanged(modified);
}

void Show::checkModifiedState() {
    if (!m_isDirty) {
        m_isDirty = true;
        emit modifiedChanged(true);
    }
}

void Show::connectCueListSignals() {
    connect(&m_cueList, &CueList::cueAdded, this, &Show::checkModifiedState);
    connect(&m_cueList, &CueList::cueRemoved, this, &Show::checkModifiedState);
    connect(&m_cueList, &CueList::cueUpdated, this, &Show::checkModifiedState);
    connect(&m_cueList, &CueList::cueMoved, this, &Show::checkModifiedState);
}

void Show::connectDcaMappingSignals() {
    connect(&m_dcaMapping, &DCAMapping::channelAssignmentChanged, this, &Show::checkModifiedState);
    connect(&m_dcaMapping, &DCAMapping::busAssignmentChanged, this, &Show::checkModifiedState);
    connect(&m_dcaMapping, &DCAMapping::mappingCleared, this, &Show::checkModifiedState);
}

void Show::connectActorLibrarySignals() {
    connect(&m_actorProfileLibrary, &ActorProfileLibrary::changed, this, &Show::checkModifiedState);
}

void Show::connectPositionLibrarySignals() {
    connect(&m_positionLibrary, &PositionLibrary::changed, this, &Show::checkModifiedState);
}

void Show::connectEnsembleLibrarySignals() {
    connect(&m_ensembleLibrary, &EnsembleLibrary::changed, this, &Show::checkModifiedState);
}

void Show::connectCueZeroSignals() {
    connect(&m_cueZero, &CueZero::changed, this, &Show::checkModifiedState);
}

void Show::setChannelGangs(const QList<QPair<int, int>>& gangs) {
    m_channelGangs = gangs;
    // keep per-gang metadata aligned by index, preserving existing entries
    while (m_channelGangMeta.size() > gangs.size())
        m_channelGangMeta.removeLast();
    while (m_channelGangMeta.size() < gangs.size())
        m_channelGangMeta.append(qMakePair(QString(), QString()));
    checkModifiedState();
}

void Show::setGangName(int index, const QString& name) {
    if (index < 0 || index >= m_channelGangMeta.size())
        return;
    m_channelGangMeta[index].first = name;
    checkModifiedState();
}

QString Show::gangName(int index) const {
    return (index >= 0 && index < m_channelGangMeta.size()) ? m_channelGangMeta[index].first
                                                            : QString();
}

void Show::setGangColor(int index, const QString& color) {
    if (index < 0 || index >= m_channelGangMeta.size())
        return;
    m_channelGangMeta[index].second = color;
    checkModifiedState();
}

QString Show::gangColor(int index) const {
    return (index >= 0 && index < m_channelGangMeta.size()) ? m_channelGangMeta[index].second
                                                            : QString();
}

void Show::newShow() {
    m_name = tr("Untitled Show");
    m_author.clear();
    m_designer.clear();
    m_notes.clear();
    m_filePath.clear();
    m_dimDcaFaders = false;
    m_selectOnSpill = false;
    m_muteDcaUnassign = false;
    m_suppressBackupSwitch = false;
    m_channelGangMeta.clear();
    m_mixerConfig = MixerConfig();
    m_mixerConfig.type = "x32";
    m_mixerConfig.port = 10023;
    m_cueList.clear();
    m_dcaMapping.clear();
    m_actorProfileLibrary.clear();
    m_positionLibrary.clear();
    m_ensembleLibrary.clear();
    m_cueZero.clear();
    m_channelGangs.clear();
    m_isDirty = false;
}

QJsonObject Show::toJson() const {
    QJsonObject json;
    json["version"] = "1.4";
    json["name"] = m_name;
    json["author"] = m_author;
    json["designer"] = m_designer;
    json["notes"] = m_notes;
    json["mixer"] = m_mixerConfig.toJson();

    json["dimDcaFaders"] = m_dimDcaFaders;
    json["selectOnSpill"] = m_selectOnSpill;
    json["muteDcaUnassign"] = m_muteDcaUnassign;
    json["suppressBackupSwitch"] = m_suppressBackupSwitch;
    json["cues"] = m_cueList.toJson();
    json["dcaMapping"] = m_dcaMapping.toJson();
    json["actors"] = m_actorProfileLibrary.toJson();
    json["positions"] = m_positionLibrary.toJson();
    json["ensembles"] = m_ensembleLibrary.toJson();
    json["cueZero"] = m_cueZero.toJson();

    QJsonArray gangArray;
    for (const auto& gang : m_channelGangs) {
        QJsonArray pair;
        pair.append(gang.first);
        pair.append(gang.second);
        gangArray.append(pair);
    }
    json["channelGangs"] = gangArray;

    // per-gang name/color, aligned by index with channelGangs
    QJsonArray gangMetaArray;
    for (const auto& meta : m_channelGangMeta) {
        QJsonObject obj;
        obj["name"] = meta.first;
        obj["color"] = meta.second;
        gangMetaArray.append(obj);
    }
    json["channelGangMeta"] = gangMetaArray;

    return json;
}

void Show::fromJson(const QJsonObject& json) {
    // version is read for forward/backward compat; 1.0/1.1 shows simply lack the
    // newer sections and fall back to cleared defaults below.
    const QString version = json["version"].toString("1.0");
    Q_UNUSED(version);

    m_name = json["name"].toString("Untitled Show");
    m_author = json["author"].toString();
    m_designer = json["designer"].toString();
    m_notes = json["notes"].toString();
    m_mixerConfig = MixerConfig::fromJson(json["mixer"].toObject());
    m_cueList.fromJson(json["cues"].toArray());

    // console-behavior preferences (added in show version 1.4)
    m_dimDcaFaders = json["dimDcaFaders"].toBool(false);
    m_selectOnSpill = json["selectOnSpill"].toBool(false);
    m_muteDcaUnassign = json["muteDcaUnassign"].toBool(false);
    m_suppressBackupSwitch = json["suppressBackupSwitch"].toBool(false);

    if (json.contains("dcaMapping")) {
        m_dcaMapping.loadFromJson(json["dcaMapping"].toObject());
    } else {
        m_dcaMapping.clear();
    }

    // actor profile library (added in show version 1.2)
    if (json.contains("actors")) {
        m_actorProfileLibrary.loadFromJson(json["actors"].toObject());
    } else {
        m_actorProfileLibrary.clear();
    }

    // named-position library (added in show version 1.2)
    if (json.contains("positions")) {
        m_positionLibrary.loadFromJson(json["positions"].toObject());
    } else {
        m_positionLibrary.clear();
    }

    // ensemble library (added in show version 1.3)
    if (json.contains("ensembles")) {
        m_ensembleLibrary.loadFromJson(json["ensembles"].toObject());
    } else {
        m_ensembleLibrary.clear();
    }

    // Cue Zero base state (added in show version 1.2)
    if (json.contains("cueZero")) {
        m_cueZero.loadFromJson(json["cueZero"].toObject());
    } else {
        m_cueZero.clear();
    }

    // ganged input-channel pairs
    m_channelGangs.clear();
    if (json.contains("channelGangs")) {
        const QJsonArray gangArray = json["channelGangs"].toArray();
        for (const QJsonValue& val : gangArray) {
            const QJsonArray pair = val.toArray();
            if (pair.size() == 2) {
                m_channelGangs.append(qMakePair(pair.at(0).toInt(), pair.at(1).toInt()));
            }
        }
    }

    // per-gang name/color, aligned by index (added in show version 1.4)
    m_channelGangMeta.clear();
    const QJsonArray gangMetaArray = json["channelGangMeta"].toArray();
    for (const QJsonValue& val : gangMetaArray) {
        const QJsonObject obj = val.toObject();
        m_channelGangMeta.append(qMakePair(obj["name"].toString(), obj["color"].toString()));
    }
    // keep metadata length aligned with the gang list
    while (m_channelGangMeta.size() < m_channelGangs.size())
        m_channelGangMeta.append(qMakePair(QString(), QString()));
    while (m_channelGangMeta.size() > m_channelGangs.size())
        m_channelGangMeta.removeLast();

    m_isDirty = false;
    emit nameChanged(m_name);
}

} // namespace OpenMix
