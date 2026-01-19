#pragma once

#include "core/Cue.h"
#include <QSortFilterProxyModel>

namespace StageBlend {

class CueFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

  public:
    explicit CueFilterProxyModel(QObject* parent = nullptr);

    // filter criteria
    void setTypeFilter(CueType type);
    void clearTypeFilter();
    bool hasTypeFilter() const { return m_filterByType; }
    CueType typeFilter() const { return m_typeFilter; }

    void setGroupFilter(const QString& group);
    void clearGroupFilter();
    QString groupFilter() const { return m_groupFilter; }

    void setTagFilter(const QString& tag);
    void clearTagFilter();
    QString tagFilter() const { return m_tagFilter; }

    void setTextFilter(const QString& text);
    void clearTextFilter();
    QString textFilter() const { return m_textFilter; }

    void clearAllFilters();

    // get available filter options from current data
    QStringList availableGroups() const;
    QStringList availableTags() const;

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

  private:
    bool m_filterByType = false;
    CueType m_typeFilter = CueType::Snapshot;
    QString m_groupFilter;
    QString m_tagFilter;
    QString m_textFilter;
};

} // namespace StageBlend
