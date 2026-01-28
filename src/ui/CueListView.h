#pragma once

#include <QAbstractItemDelegate>
#include <QAction>
#include <QTableView>
#include <QWidget>

namespace OpenMix {

class Application;
class Cue;
class CueTableModel;
class CueFilterProxyModel;
class CueFilterBar;
class CueNumberDelegate;
class CueTypeDelegate;
class CueTextDelegate;

class CueListView : public QWidget {
    Q_OBJECT

  public:
    explicit CueListView(Application* app, QWidget* parent = nullptr);

    int selectedCueIndex() const;
    void setCurrentCueHighlight(int index);
    void setStandbyCueHighlight(int index);
    void refreshAll();
    void refreshCurrentCue();

    // access to filter bar for external updates
    CueFilterBar* filterBar() const { return m_filterBar; }

    // access to model for live edit highlighting
    CueTableModel* model() const { return m_model; }

  public slots:
    void addNewCue();
    void deleteSelectedCue();
    void duplicateSelectedCue();

  signals:
    void cueSelected(int index);
    void cueDoubleClicked(int index);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private slots:
    void onSelectionChanged();
    void onCueReordered(int fromIndex, int toIndex);
    void onFiltersChanged();
    void onTabNavigationRequested(const QModelIndex& fromIndex, bool forward);

  private:
    void setupUi();
    void setupDelegates();
    void createActions();
    void editNextCell(bool forward);
    QModelIndex nextEditableIndex(const QModelIndex& current, bool forward) const;

    Application* m_app;
    QTableView* m_tableView;
    CueTableModel* m_model;
    CueFilterProxyModel* m_proxyModel;
    CueFilterBar* m_filterBar;
    int m_currentCueIndex = -1;
    int m_standbyCueIndex = -1;

    // delegates
    CueNumberDelegate* m_numberDelegate;
    CueTypeDelegate* m_typeDelegate;
    CueTextDelegate* m_textDelegate;

    // tab navigation guard
    bool m_tabNavigationPending = false;

    // track if we were just editing
    bool m_wasEditing = false;

    // actions
    QAction* m_duplicateCueAction;
};

} // namespace OpenMix
