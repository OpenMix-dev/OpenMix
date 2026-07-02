#include "CueFilterProxyModel.h"
#include "CueTableModel.h"
#include "core/CueList.h"

namespace OpenMix {

CueFilterProxyModel::CueFilterProxyModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void CueFilterProxyModel::setTypeFilter(CueType type) {
    beginFilterChange();
    m_filterByType = true;
    m_typeFilter = type;
    endFilterChange();
}

void CueFilterProxyModel::clearTypeFilter() {
    beginFilterChange();
    m_filterByType = false;
    endFilterChange();
}

void CueFilterProxyModel::setGroupFilter(const QString& group) {
    beginFilterChange();
    m_groupFilter = group;
    endFilterChange();
}

void CueFilterProxyModel::clearGroupFilter() {
    beginFilterChange();
    m_groupFilter.clear();
    endFilterChange();
}

void CueFilterProxyModel::setTagFilter(const QString& tag) {
    beginFilterChange();
    m_tagFilter = tag;
    endFilterChange();
}

void CueFilterProxyModel::clearTagFilter() {
    beginFilterChange();
    m_tagFilter.clear();
    endFilterChange();
}

void CueFilterProxyModel::setTextFilter(const QString& text) {
    beginFilterChange();
    m_textFilter = text;
    endFilterChange();
}

void CueFilterProxyModel::clearTextFilter() {
    beginFilterChange();
    m_textFilter.clear();
    endFilterChange();
}

void CueFilterProxyModel::clearAllFilters() {
    beginFilterChange();
    m_filterByType = false;
    m_groupFilter.clear();
    m_tagFilter.clear();
    m_textFilter.clear();
    endFilterChange();
}

QStringList CueFilterProxyModel::availableGroups() const {
    QSet<QString> groups;

    CueTableModel* model = qobject_cast<CueTableModel*>(sourceModel());
    if (!model || !model->cueList())
        return QStringList();

    CueList* cueList = model->cueList();
    for (const Cue& cue : *cueList) {
        const QString& group = cue.group();
        if (!group.isEmpty()) {
            groups.insert(group);
        }
    }

    QStringList result = groups.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList CueFilterProxyModel::availableTags() const {
    QSet<QString> tags;

    CueTableModel* model = qobject_cast<CueTableModel*>(sourceModel());
    if (!model || !model->cueList())
        return QStringList();

    CueList* cueList = model->cueList();
    for (const Cue& cue : *cueList) {
        const QStringList& cueTags = cue.tags();
        for (const QString& tag : cueTags) {
            if (!tag.isEmpty()) {
                tags.insert(tag);
            }
        }
    }

    QStringList result = tags.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

bool CueFilterProxyModel::filterAcceptsRow(int sourceRow, [[maybe_unused]] const QModelIndex& sourceParent) const {

    CueTableModel* model = qobject_cast<CueTableModel*>(sourceModel());
    if (!model)
        return true;

    // The running and standby cues stay visible regardless of filters, so an
    // operator never loses sight of what is playing or queued next.
    if (sourceRow == model->currentCueIndex() || sourceRow == model->standbyCueIndex())
        return true;

    const Cue* cue = model->cueAt(sourceRow);
    if (!cue)
        return true;

    // type filter
    if (m_filterByType && cue->type() != m_typeFilter) {
        return false;
    }

    // group filter
    if (!m_groupFilter.isEmpty()) {
        if (cue->group().compare(m_groupFilter, Qt::CaseInsensitive) != 0) {
            return false;
        }
    }

    // tag filter
    if (!m_tagFilter.isEmpty()) {
        bool hasTag = false;
        for (const QString& tag : cue->tags()) {
            if (tag.compare(m_tagFilter, Qt::CaseInsensitive) == 0) {
                hasTag = true;
                break;
            }
        }
        if (!hasTag)
            return false;
    }

    // text filter (searches name & notes)
    if (!m_textFilter.isEmpty()) {
        bool matchesText = cue->name().contains(m_textFilter, Qt::CaseInsensitive) ||
                           cue->notes().contains(m_textFilter, Qt::CaseInsensitive) ||
                           cue->group().contains(m_textFilter, Qt::CaseInsensitive);

        // also search tags
        if (!matchesText) {
            for (const QString& tag : cue->tags()) {
                if (tag.contains(m_textFilter, Qt::CaseInsensitive)) {
                    matchesText = true;
                    break;
                }
            }
        }

        if (!matchesText)
            return false;
    }

    return true;
}

bool CueFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    // sort by cue number by default
    CueTableModel* model = qobject_cast<CueTableModel*>(sourceModel());
    if (!model)
        return QSortFilterProxyModel::lessThan(left, right);

    const Cue* leftCue = model->cueAt(left.row());
    const Cue* rightCue = model->cueAt(right.row());

    if (!leftCue || !rightCue) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    // if sorting by column 0 (cue number), use numeric comparison
    if (left.column() == CueTableModel::ColNumber) {
        return leftCue->number() < rightCue->number();
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

} // namespace OpenMix
