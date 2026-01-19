#include "CueList.h"
#include <algorithm>

namespace StageBlend {

CueList::CueList(QObject* parent) : QObject(parent) {}

int CueList::indexOf(const QString& id) const {
    for (int i = 0; i < m_cues.size(); ++i) {
        if (m_cues[i].id() == id) {
            return i;
        }
    }
    return -1;
}

int CueList::indexOfNumber(double number) const {
    for (int i = 0; i < m_cues.size(); ++i) {
        if (qFuzzyCompare(m_cues[i].number(), number)) {
            return i;
        }
    }
    return -1;
}

Cue* CueList::findById(const QString& id) {
    int idx = indexOf(id);
    return idx >= 0 ? &m_cues[idx] : nullptr;
}

const Cue* CueList::findById(const QString& id) const {
    int idx = indexOf(id);
    return idx >= 0 ? &m_cues[idx] : nullptr;
}

Cue* CueList::findByNumber(double number) {
    int idx = indexOfNumber(number);
    return idx >= 0 ? &m_cues[idx] : nullptr;
}

void CueList::addCue(const Cue& cue) {
    m_cues.append(cue);
    emit cueAdded(m_cues.size() - 1);
}

void CueList::insertCue(int index, const Cue& cue) {
    if (index < 0)
        index = 0;
    if (index > m_cues.size())
        index = m_cues.size();
    m_cues.insert(index, cue);
    emit cueAdded(index);
}

void CueList::updateCue(int index, const Cue& cue) {
    if (index >= 0 && index < m_cues.size()) {
        m_cues[index] = cue;
        emit cueUpdated(index);
    }
}

void CueList::removeCue(int index) {
    if (index >= 0 && index < m_cues.size()) {
        m_cues.remove(index);
        emit cueRemoved(index);
    }
}

void CueList::removeCueById(const QString& id) {
    int idx = indexOf(id);
    if (idx >= 0) {
        removeCue(idx);
    }
}

void CueList::moveCue(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= m_cues.size())
        return;
    if (toIndex < 0 || toIndex >= m_cues.size())
        return;
    if (fromIndex == toIndex)
        return;

    Cue cue = m_cues.takeAt(fromIndex);
    m_cues.insert(toIndex, cue);
    emit cueMoved(fromIndex, toIndex);
}

void CueList::clear() {
    m_cues.clear();
    emit listCleared();
}

void CueList::sortByNumber() { std::sort(m_cues.begin(), m_cues.end()); }

double CueList::nextCueNumber() const {
    if (m_cues.isEmpty()) {
        return 1.0;
    }
    double maxNum = 0.0;
    for (const auto& cue : m_cues) {
        if (cue.number() > maxNum) {
            maxNum = cue.number();
        }
    }
    return std::floor(maxNum) + 1.0;
}

QJsonArray CueList::toJson() const {
    QJsonArray arr;
    for (const auto& cue : m_cues) {
        arr.append(cue.toJson());
    }
    return arr;
}

void CueList::fromJson(const QJsonArray& json) {
    clear();
    for (const auto& val : json) {
        m_cues.append(Cue::fromJson(val.toObject()));
    }
}

} // namespace StageBlend
