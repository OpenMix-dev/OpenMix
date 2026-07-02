#include "CueZero.h"
#include "protocol/MixerProtocol.h"

namespace OpenMix {

CueZero::CueZero(QObject* parent) : QObject(parent) {}

void CueZero::setBaseScene(int scene) {
    if (m_baseScene != scene) {
        m_baseScene = scene;
        emit changed();
    }
}

void CueZero::setLevels(const QJsonObject& levels) {
    m_levels = levels;
    emit changed();
}

void CueZero::setLevel(const QString& path, const QVariant& value) {
    m_levels[path] = QJsonValue::fromVariant(value);
    emit changed();
}

void CueZero::setLabels(const QJsonObject& labels) {
    m_labels = labels;
    emit changed();
}

void CueZero::setLabel(const QString& path, const QString& name) {
    m_labels[path] = name;
    emit changed();
}

bool CueZero::isEmpty() const {
    return m_baseScene < 0 && m_levels.isEmpty() && m_labels.isEmpty();
}

void CueZero::clear() {
    m_baseScene = -1;
    m_levels = QJsonObject();
    m_labels = QJsonObject();
    emit changed();
}

void CueZero::apply(MixerProtocol* mixer) const {
    if (!mixer || !mixer->isConnected()) {
        return;
    }

    // recall the base scene first so subsequent overrides win
    if (m_baseScene >= 0) {
        mixer->recallScene(m_baseScene);
    }

    for (const auto& [path, value] : m_levels.asKeyValueRange()) {
        mixer->sendParameter(path.toString(), value.toVariant());
    }
    for (const auto& [path, value] : m_labels.asKeyValueRange()) {
        mixer->sendParameter(path.toString(), value.toVariant());
    }
}

QJsonObject CueZero::safeValues() const {
    QJsonObject values = m_levels;
    for (const auto& [path, value] : m_labels.asKeyValueRange()) {
        values[path.toString()] = value;
    }
    return values;
}

QJsonObject CueZero::toJson() const {
    QJsonObject json;
    json["baseScene"] = m_baseScene;
    json["levels"] = m_levels;
    json["labels"] = m_labels;
    return json;
}

void CueZero::loadFromJson(const QJsonObject& json) {
    m_baseScene = json.contains("baseScene") ? json["baseScene"].toInt(-1) : -1;
    m_levels = json["levels"].toObject();
    m_labels = json["labels"].toObject();
}

} // namespace OpenMix
