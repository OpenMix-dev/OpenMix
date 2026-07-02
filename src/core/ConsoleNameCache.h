#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

namespace OpenMix {

// Cache of console-reported snippet and scene names, keyed by index. Populated
// from the console (name query) and persisted with the show so the UI can label
// snippets/scenes even while offline. Mirrors the console's snippet/scene name
// tables.
class ConsoleNameCache : public QObject {
    Q_OBJECT

  public:
    explicit ConsoleNameCache(QObject* parent = nullptr);

    void setSnippetName(int index, const QString& name);
    [[nodiscard]] QString snippetName(int index) const { return m_snippets.value(index); }
    [[nodiscard]] QMap<int, QString> snippetNames() const { return m_snippets; }

    void setSceneName(int index, const QString& name);
    [[nodiscard]] QString sceneName(int index) const { return m_scenes.value(index); }
    [[nodiscard]] QMap<int, QString> sceneNames() const { return m_scenes; }

    [[nodiscard]] bool isEmpty() const { return m_snippets.isEmpty() && m_scenes.isEmpty(); }
    void clear();

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed();

  private:
    QMap<int, QString> m_snippets; // snippet index -> name
    QMap<int, QString> m_scenes;   // scene index -> name
};

} // namespace OpenMix
