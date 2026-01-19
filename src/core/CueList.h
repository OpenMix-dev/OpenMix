#pragma once

#include "Cue.h"
#include <QJsonArray>
#include <QObject>
#include <QVector>

namespace StageBlend {

class CueList : public QObject {
    Q_OBJECT

  public:
    explicit CueList(QObject* parent = nullptr);

    // cue access
    int count() const { return m_cues.size(); }
    bool isEmpty() const { return m_cues.isEmpty(); }

    const Cue& at(int index) const { return m_cues.at(index); }
    Cue& operator[](int index) { return m_cues[index]; }
    const Cue& operator[](int index) const { return m_cues[index]; }

    // find cue
    int indexOf(const QString& id) const;
    int indexOfNumber(double number) const;
    Cue* findById(const QString& id);
    const Cue* findById(const QString& id) const;
    Cue* findByNumber(double number);

    // cue management
    void addCue(const Cue& cue);
    void insertCue(int index, const Cue& cue);
    void updateCue(int index, const Cue& cue);
    void removeCue(int index);
    void removeCueById(const QString& id);
    void moveCue(int fromIndex, int toIndex);
    void clear();

    // sorting
    void sortByNumber();

    // generate next cue number
    double nextCueNumber() const;

    // iteration
    QVector<Cue>::iterator begin() { return m_cues.begin(); }
    QVector<Cue>::iterator end() { return m_cues.end(); }
    QVector<Cue>::const_iterator begin() const { return m_cues.begin(); }
    QVector<Cue>::const_iterator end() const { return m_cues.end(); }

    // serialization
    QJsonArray toJson() const;
    void fromJson(const QJsonArray& json);

  signals:
    void cueAdded(int index);
    void cueRemoved(int index);
    void cueUpdated(int index);
    void cueMoved(int fromIndex, int toIndex);
    void listCleared();

  private:
    QVector<Cue> m_cues;
};

} // namespace StageBlend
