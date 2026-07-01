#pragma once

#include "Cue.h"
#include <QJsonArray>
#include <QObject>
#include <QVector>
#include <optional>

namespace OpenMix {

class CueList : public QObject {
    Q_OBJECT

  public:
    explicit CueList(QObject* parent = nullptr);

    [[nodiscard]] int count() const noexcept { return m_cues.size(); }
    [[nodiscard]] bool isEmpty() const noexcept { return m_cues.isEmpty(); }

    [[nodiscard]] const Cue& at(int index) const { return m_cues.at(index); }
    [[nodiscard]] Cue& operator[](int index) { return m_cues[index]; }
    [[nodiscard]] const Cue& operator[](int index) const { return m_cues[index]; }

    [[nodiscard]] std::optional<int> indexOf(const QString& id) const;
    [[nodiscard]] std::optional<int> indexOfNumber(double number) const;
    [[nodiscard]] Cue* findById(const QString& id);
    [[nodiscard]] const Cue* findById(const QString& id) const;
    [[nodiscard]] Cue* findByNumber(double number);

    void addCue(const Cue& cue);
    void insertCue(int index, const Cue& cue);
    void updateCue(int index, const Cue& cue);
    void removeCue(int index);
    void removeCueById(const QString& id);
    void moveCue(int fromIndex, int toIndex);
    void clear();

    void sortByNumber();

    [[nodiscard]] double nextCueNumber() const;

    QVector<Cue>::iterator begin() { return m_cues.begin(); }
    QVector<Cue>::iterator end() { return m_cues.end(); }
    QVector<Cue>::const_iterator begin() const { return m_cues.begin(); }
    QVector<Cue>::const_iterator end() const { return m_cues.end(); }

    QJsonArray toJson() const;
    void fromJson(const QJsonArray& json);

  signals:
    void cueAboutToBeAdded(int index); // emitted before the cue is inserted
    void cueAdded(int index);
    void cueAboutToBeRemoved(int index); // emitted before the cue is erased
    void cueRemoved(int index);
    void cueUpdated(int index);
    void cueAboutToBeMoved(int fromIndex, int toIndex); // emitted before the move
    void cueMoved(int fromIndex, int toIndex);
    void listCleared();
    void listLoaded();

  private:
    QVector<Cue> m_cues;
};

} // namespace OpenMix
