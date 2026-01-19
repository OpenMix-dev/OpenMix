#include "Show.h"

namespace StageBlend {

QJsonObject MixerConfig::toJson() const {
    QJsonObject json;
    json["type"] = type;
    json["host"] = host;
    json["port"] = port;
    return json;
}

MixerConfig MixerConfig::fromJson(const QJsonObject& json) {
    MixerConfig config;
    config.type = json["type"].toString("x32");
    config.host = json["host"].toString();
    config.port = json["port"].toInt(10023);
    return config;
}

Show::Show(QObject* parent) : QObject(parent), m_cueList(this) {
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
    m_modified = false;
}

QJsonObject Show::toJson() const {
    QJsonObject json;
    json["version"] = "1.0";
    json["name"] = m_name;
    json["author"] = m_author;
    json["notes"] = m_notes;
    json["mixer"] = m_mixerConfig.toJson();
    json["cues"] = m_cueList.toJson();
    return json;
}

void Show::fromJson(const QJsonObject& json) {
    m_name = json["name"].toString("Untitled Show");
    m_author = json["author"].toString();
    m_notes = json["notes"].toString();
    m_mixerConfig = MixerConfig::fromJson(json["mixer"].toObject());
    m_cueList.fromJson(json["cues"].toArray());
    m_modified = false;
    emit nameChanged(m_name);
}

} // namespace StageBlend
