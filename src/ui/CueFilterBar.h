#pragma once

#include <QAction>
#include <QWidget>

class QLineEdit;
class QComboBox;
class QPushButton;

namespace OpenMix {

class Application;
class CueFilterProxyModel;

class CueFilterBar : public QWidget {
    Q_OBJECT

  public:
    explicit CueFilterBar(CueFilterProxyModel* proxyModel, QWidget* parent = nullptr);

    void updateFilterOptions();

    QAction* clearFiltersAction() const { return m_clearFiltersAction; }

  signals:
    void filtersChanged();

  private slots:
    void onSearchTextChanged(const QString& text);
    void onTypeFilterChanged(int index);
    void onGroupFilterChanged(int index);
    void onTagFilterChanged(int index);
    void onClearFilters();

  private:
    void setupUi();
    void populateTypeCombo();

    CueFilterProxyModel* m_proxyModel;
    QLineEdit* m_searchEdit;
    QComboBox* m_typeCombo;
    QComboBox* m_groupCombo;
    QComboBox* m_tagCombo;
    QPushButton* m_clearButton;

    // actions
    QAction* m_clearFiltersAction;
};

} // namespace OpenMix
