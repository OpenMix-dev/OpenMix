#include "Show.h"

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

void Show::newShow() {
    m_name = tr("Untitled Show");
    m_author.clear();
    m_notes.clear();
    m_filePath.clear();
    m_mixerConfig = MixerConfig();
    m_mixerConfig.type = "x32";
    m_mixerConfig.port = 10023;
    m_cueList.clear();
    m_dcaMapping.clear();
    m_actorProfileLibrary.clear();
    m_positionLibrary.clear();
    m_ensembleLibrary.clear();
    m_cueZero.clear();
    m_isDirty = false;
}

QJsonObject Show::toJson() const {
    QJsonObject json;
    json["version"] = "1.3";
    json["name"] = m_name;
    json["author"] = m_author;
    json["notes"] = m_notes;
    json["mixer"] = m_mixerConfig.toJson();
    json["cues"] = m_cueList.toJson();
    json["dcaMapping"] = m_dcaMapping.toJson();
    json["actors"] = m_actorProfileLibrary.toJson();
    json["positions"] = m_positionLibrary.toJson();
    json["ensembles"] = m_ensembleLibrary.toJson();
    json["cueZero"] = m_cueZero.toJson();
    return json;
}

void Show::fromJson(const QJsonObject& json) {
    // version is read for forward/backward compat; 1.0/1.1 shows simply lack the
    // newer sections and fall back to cleared defaults below.
    const QString version = json["version"].toString("1.0");
    Q_UNUSED(version);

    m_name = json["name"].toString("Untitled Show");
    m_author = json["author"].toString();
    m_notes = json["notes"].toString();
    m_mixerConfig = MixerConfig::fromJson(json["mixer"].toObject());
    m_cueList.fromJson(json["cues"].toArray());

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

    m_isDirty = false;
    emit nameChanged(m_name);
}

} // namespace OpenMix
