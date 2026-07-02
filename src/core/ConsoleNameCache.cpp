#include "ConsoleNameCache.h"
#include <QJsonObject>

namespace OpenMix {

ConsoleNameCache::ConsoleNameCache(QObject* parent) : QObject(parent) {}

void ConsoleNameCache::setSnippetName(int index, const QString& name) {
    if (m_snippets.value(index) == name && m_snippets.contains(index))
        return;
    m_snippets[index] = name;
    emit changed();
}

void ConsoleNameCache::setSceneName(int index, const QString& name) {
    if (m_scenes.value(index) == name && m_scenes.contains(index))
        return;
    m_scenes[index] = name;
    emit changed();
}

void ConsoleNameCache::clear() {
    if (isEmpty())
        return;
    m_snippets.clear();
    m_scenes.clear();
    emit changed();
}

QJsonObject ConsoleNameCache::toJson() const {
    QJsonObject snippets;
    for (auto it = m_snippets.begin(); it != m_snippets.end(); ++it)
        snippets[QString::number(it.key())] = it.value();
    QJsonObject scenes;
    for (auto it = m_scenes.begin(); it != m_scenes.end(); ++it)
        scenes[QString::number(it.key())] = it.value();

    QJsonObject json;
    json["snippets"] = snippets;
    json["scenes"] = scenes;
    return json;
}

void ConsoleNameCache::loadFromJson(const QJsonObject& json) {
    m_snippets.clear();
    m_scenes.clear();
    const QJsonObject snippets = json.value("snippets").toObject();
    for (auto it = snippets.begin(); it != snippets.end(); ++it)
        m_snippets[it.key().toInt()] = it.value().toString();
    const QJsonObject scenes = json.value("scenes").toObject();
    for (auto it = scenes.begin(); it != scenes.end(); ++it)
        m_scenes[it.key().toInt()] = it.value().toString();
    emit changed();
}

} // namespace OpenMix
