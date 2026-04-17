#include "CueList.h"
#include <algorithm>
#include <iterator>

namespace OpenMix {

namespace {
constexpr double FIRST_CUE_NUMBER = 1.0;
} // namespace

CueList::CueList(QObject* parent) : QObject(parent) {}

std::optional<int> CueList::indexOf(const QString& id) const {
    auto it = std::find_if(m_cues.cbegin(), m_cues.cend(),
                           [&id](const Cue& c) { return c.id() == id; });
    if (it == m_cues.cend())
        return std::nullopt;
    return static_cast<int>(std::distance(m_cues.cbegin(), it));
}

std::optional<int> CueList::indexOfNumber(double number) const {
    auto it = std::find_if(m_cues.cbegin(), m_cues.cend(),
                           [number](const Cue& c) { return qFuzzyCompare(c.number(), number); });
    if (it == m_cues.cend())
        return std::nullopt;
    return static_cast<int>(std::distance(m_cues.cbegin(), it));
}

Cue* CueList::findById(const QString& id) {
    const auto idx = indexOf(id);
    return idx ? &m_cues[*idx] : nullptr;
}

const Cue* CueList::findById(const QString& id) const {
    const auto idx = indexOf(id);
    return idx ? &m_cues[*idx] : nullptr;
}

Cue* CueList::findByNumber(double number) {
    const auto idx = indexOfNumber(number);
    return idx ? &m_cues[*idx] : nullptr;
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
    if (const auto idx = indexOf(id)) {
        removeCue(*idx);
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
    if (m_cues.isEmpty())
        return FIRST_CUE_NUMBER;
    auto it = std::max_element(m_cues.cbegin(), m_cues.cend(),
                               [](const Cue& a, const Cue& b) { return a.number() < b.number(); });
    return std::floor(it->number()) + FIRST_CUE_NUMBER;
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
    emit listLoaded();
}

} // namespace OpenMix
