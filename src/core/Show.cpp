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

Show::Show(QObject* parent) : QObject(parent), m_cueList(this), m_dcaMapping(this) {
    connectCueListSignals();
    newShow();
}

void Show::setName(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit nameChanged(name);
        setModified(true);
    }
}

void Show::setModified(bool modified) {
    if (m_modified != modified) {
        m_modified = modified;
        emit modifiedChanged(modified);
    }
}

void Show::connectCueListSignals() {
    connect(&m_cueList, &CueList::cueAdded, this, [this]() { setModified(true); });
    connect(&m_cueList, &CueList::cueRemoved, this, [this]() { setModified(true); });
    connect(&m_cueList, &CueList::cueUpdated, this, [this]() { setModified(true); });
    connect(&m_cueList, &CueList::cueMoved, this, [this]() { setModified(true); });
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
    m_modified = false;
}

QJsonObject Show::toJson() const {
    QJsonObject json;
    json["version"] = "1.1";
    json["name"] = m_name;
    json["author"] = m_author;
    json["notes"] = m_notes;
    json["mixer"] = m_mixerConfig.toJson();
    json["cues"] = m_cueList.toJson();
    json["dcaMapping"] = m_dcaMapping.toJson();
    return json;
}

void Show::fromJson(const QJsonObject& json) {
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

    m_modified = false;
    emit nameChanged(m_name);
}

} // namespace OpenMix
