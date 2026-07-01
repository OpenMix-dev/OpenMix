#pragma once

#include "core/Cue.h"
#include <QAbstractItemDelegate>
#include <QAction>
#include <QTableView>
#include <QWidget>
#include <optional>

namespace OpenMix {

class Application;
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
    void duplicateSelectedCue(); // clone the selected cue to the end of the list
    void cloneCueAfter();        // clone the selected cue in place, just after it
    void copySelectedCue();
    void pasteCue();      // insert the clipboard cue as a new cue after selection
    void pasteCueMerge(); // merge clipboard content into the selected cue
    void pasteCueSwap();  // exchange content between clipboard and selected cue
    void fillDown();      // copy the selected cue's content into the next cue
    void cloneOffsets();  // copy just the selected cue's level offsets to the next cue
    void jumpToSelectedCue(); // set the selected cue as standby without firing
    void setEditingLocked(bool locked); // make the cue table read-only
    void setRowHeight(int pixels);      // cue-table row height
    void setColumnVisible(int column, bool visible);

    [[nodiscard]] bool hasClipboardCue() const { return m_clipboard.has_value(); }

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
    void insertCueAt(int index, const Cue& cue); // undoable insert + select
    void selectSourceRow(int sourceRow);

    std::optional<Cue> m_clipboard; // copied cue for paste operations

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
