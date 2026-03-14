#include "CueListView.h"
#include "CueFilterBar.h"
#include "CueFilterProxyModel.h"
#include "CueItemDelegates.h"
#include "CueTableModel.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/ShortcutManager.h"
#include "core/Show.h"
#include "core/UndoCommands.h"

#include <QApplication>
#include <QFocusEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace OpenMix {

CueListView::CueListView(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    setupDelegates();
    createActions();

    // connect playback engine for highlighting
    connect(m_app->playbackEngine(), &PlaybackEngine::currentCueChanged, this,
            &CueListView::setCurrentCueHighlight);
    connect(m_app->playbackEngine(), &PlaybackEngine::standbyCueChanged, this,
            &CueListView::setStandbyCueHighlight);

    // connect model signals for undo integration
    connect(m_model, &CueTableModel::cueReordered, this, &CueListView::onCueReordered);

    // install event filter
    m_tableView->installEventFilter(this);
    m_tableView->viewport()->installEventFilter(this);
}

void CueListView::setupUi() {
    setMinimumSize(300, 250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // create model & proxy
    m_model = new CueTableModel(m_app->show()->cueList(), this);
    m_proxyModel = new CueFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    // create filter bar
    m_filterBar = new CueFilterBar(m_proxyModel, this);
    connect(m_filterBar, &CueFilterBar::filtersChanged, this, &CueListView::onFiltersChanged);

    // create table view
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_proxyModel);

    // selection & editing
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::DoubleClicked |
                                 QAbstractItemView::EditKeyPressed |
                                 QAbstractItemView::SelectedClicked);

    m_tableView->setAlternatingRowColors(true);
    m_tableView->setTabKeyNavigation(false);

    // drag & drop
    m_tableView->setDragEnabled(true);
    m_tableView->setAcceptDrops(true);
    m_tableView->setDropIndicatorShown(true);
    m_tableView->setDragDropMode(QAbstractItemView::InternalMove);
    m_tableView->setDefaultDropAction(Qt::MoveAction);

    // headers
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setShowGrid(false);

    // column widths
    m_tableView->setColumnWidth(CueTableModel::ColNumber, 60);
    m_tableView->setColumnWidth(CueTableModel::ColName, 150);
    m_tableView->setColumnWidth(CueTableModel::ColType, 100);
    m_tableView->setColumnWidth(CueTableModel::ColGroup, 100);
    m_tableView->setColumnWidth(CueTableModel::ColTags, 120);

    // connections
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CueListView::onSelectionChanged);

    // update filter options when group/tags are edited
    connect(
        m_model, &CueTableModel::cueEditRequested, this,
        [this](int, int column, const QVariant&, const QVariant&) {
            if (column == CueTableModel::ColGroup || column == CueTableModel::ColTags) {
                m_filterBar->updateFilterOptions();
            }
        },
        Qt::QueuedConnection);

    layout->addWidget(m_filterBar);
    layout->addWidget(m_tableView);
}

void CueListView::setupDelegates() {
    CueList* cueList = m_app->show()->cueList();

    // create delegates
    m_numberDelegate = new CueNumberDelegate(cueList, this);
    m_typeDelegate = new CueTypeDelegate(this);
    m_textDelegate = new CueTextDelegate(this);

    // assign delegates to columns
    m_tableView->setItemDelegateForColumn(CueTableModel::ColNumber, m_numberDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColName, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColType, m_typeDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColGroup, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColTags, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColNotes, m_textDelegate);

    // connect tab navigation
    connect(m_numberDelegate, &CueNumberDelegate::tabNavigationRequested, this,
            &CueListView::onTabNavigationRequested);

    connect(m_typeDelegate, &CueTypeDelegate::tabNavigationRequested, this,
            &CueListView::onTabNavigationRequested);

    connect(m_textDelegate, &CueTextDelegate::tabNavigationRequested, this,
            &CueListView::onTabNavigationRequested);
}

void CueListView::createActions() {
    m_duplicateCueAction = new QAction(tr("Duplicate Cue"), this);
    m_duplicateCueAction->setToolTip(tr("Duplicate the selected cue"));
    m_duplicateCueAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_duplicateCueAction, &QAction::triggered, this, &CueListView::duplicateSelectedCue);
    addAction(m_duplicateCueAction);

    // register with shortcut manager
    ShortcutManager* sm = m_app->shortcutManager();
    sm->registerAction("edit.duplicateCue", m_duplicateCueAction, QKeySequence());

    // register filter bar clear action
    QAction* clearAction = m_filterBar->clearFiltersAction();
    clearAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(clearAction);
    sm->registerAction("view.clearFilters", clearAction, QKeySequence());
}

int CueListView::selectedCueIndex() const {
    QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid())
        return -1;

    // map from proxy to source
    QModelIndex sourceIndex = m_proxyModel->mapToSource(current);
    return sourceIndex.row();
}

void CueListView::setCurrentCueHighlight(int index) {
    m_currentCueIndex = index;
    m_model->setCurrentCueIndex(index);

    // auto-scroll to current cue
    if (index >= 0) {
        QModelIndex sourceIndex = m_model->index(index, 0);
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
        if (proxyIndex.isValid()) {
            m_tableView->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
        }
    }
}

void CueListView::setStandbyCueHighlight(int index) {
    m_standbyCueIndex = index;
    m_model->setStandbyCueIndex(index);

    if (m_app->playbackEngine()->state() == PlaybackState::Stopped && index >= 0) {
        QModelIndex sourceIndex = m_model->index(index, 0);
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
        if (proxyIndex.isValid()) {
            m_tableView->selectRow(proxyIndex.row());
            m_tableView->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
        }
    }
}

void CueListView::refreshAll() {
    // update filter options
    m_filterBar->updateFilterOptions();
}

void CueListView::refreshCurrentCue() {
    int idx = selectedCueIndex();
    if (idx >= 0) {
        // tell model to emit dataChanged for this row
        QModelIndex topLeft = m_model->index(idx, 0);
        QModelIndex bottomRight = m_model->index(idx, m_model->columnCount() - 1);
        emit m_model->dataChanged(topLeft, bottomRight);
    }
}

void CueListView::addNewCue() {
    CueList* cueList = m_app->show()->cueList();
    double nextNum = cueList->nextCueNumber();
    Cue cue(nextNum);

    // push to undo stack
    m_app->undoStack()->push(new AddCueCommand(cueList, cue));

    // manually add cue since undo command's first redo is skipped
    cueList->addCue(cue);

    // select the new cue
    int newRow = cueList->count() - 1;
    QModelIndex sourceIndex = m_model->index(newRow, 0);
    QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
    if (proxyIndex.isValid()) {
        m_tableView->selectRow(proxyIndex.row());
    }

    m_filterBar->updateFilterOptions();
}

void CueListView::deleteSelectedCue() {
    int idx = selectedCueIndex();
    if (idx < 0)
        return;

    CueList* cueList = m_app->show()->cueList();

    // push to undo stack
    m_app->undoStack()->push(new RemoveCueCommand(cueList, idx));

    // manually remove since undo command's first redo is skipped
    cueList->removeCue(idx);

    m_filterBar->updateFilterOptions();
}

void CueListView::duplicateSelectedCue() {
    int idx = selectedCueIndex();
    if (idx < 0)
        return;

    CueList* cueList = m_app->show()->cueList();
    Cue original = cueList->at(idx);

    // create duplicate w/ next available number & new ID
    Cue duplicate = original;
    duplicate.regenerateId();
    duplicate.setNumber(cueList->nextCueNumber());

    // push to undo stack
    m_app->undoStack()->push(new AddCueCommand(cueList, duplicate));

    // manually add cue
    cueList->addCue(duplicate);

    // select the new cue
    int newRow = cueList->count() - 1;
    QModelIndex sourceIndex = m_model->index(newRow, 0);
    QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
    if (proxyIndex.isValid()) {
        m_tableView->selectRow(proxyIndex.row());
    }

    m_filterBar->updateFilterOptions();
}

void CueListView::onSelectionChanged() { emit cueSelected(selectedCueIndex()); }

void CueListView::onCueReordered(int fromIndex, int toIndex) {
    CueList* cueList = m_app->show()->cueList();

    // push to undo stack
    m_app->undoStack()->push(new MoveCueCommand(cueList, fromIndex, toIndex));

    // manually move cue
    cueList->moveCue(fromIndex, toIndex);
}

void CueListView::onFiltersChanged() {
    // could emit a signal or update status
}

void CueListView::onTabNavigationRequested(const QModelIndex& fromIndex, bool forward) {
    if (m_tabNavigationPending) {
        return;
    }
    m_tabNavigationPending = true;

    // find next editable cell
    QModelIndex next = nextEditableIndex(fromIndex, forward);
    if (next.isValid()) {
        m_tableView->setFocus();
        m_tableView->setCurrentIndex(next);
        m_tableView->scrollTo(next);
        QTimer::singleShot(10, this, [this, next]() {
            m_tabNavigationPending = false;
            if (next.isValid()) {
                m_tableView->edit(next);
            }
        });
    } else {
        m_tabNavigationPending = false;
    }
}

bool CueListView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_tableView && event->type() == QEvent::FocusOut) {
        QFocusEvent* focusEvent = static_cast<QFocusEvent*>(event);
        if (focusEvent->reason() != Qt::PopupFocusReason &&
            focusEvent->reason() != Qt::ActiveWindowFocusReason) {
            QWidget* newFocus = QApplication::focusWidget();
            if (!newFocus || !m_tableView->isAncestorOf(newFocus)) {
                m_wasEditing = false;
            }
        }
    }

    if (watched == m_tableView && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        QModelIndex current = m_tableView->currentIndex();

        // check if there's an active editor by looking for index widget
        bool isEditing = current.isValid() && m_tableView->indexWidget(current) != nullptr;

        // handle enter to edit current cell
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (current.isValid() && !isEditing) {
                Qt::ItemFlags flags = m_proxyModel->flags(current);
                if (flags & Qt::ItemIsEditable) {
                    m_wasEditing = true;
                    m_tableView->edit(current);
                    return true;
                }
            }
        }

        // handle tab/shift+tab for navigation
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            if (current.isValid() && !isEditing) {
                bool forward = (keyEvent->key() == Qt::Key_Tab);

                QModelIndex targetCell;
                if (m_wasEditing) {
                    // advance to next cell
                    targetCell = nextEditableIndex(current, forward);
                } else {
                    // start at first/last column
                    int row = current.row();
                    int col = forward ? CueTableModel::ColNumber : CueTableModel::ColNotes;
                    targetCell = m_proxyModel->index(row, col);
                }

                if (targetCell.isValid()) {
                    m_wasEditing = true;
                    m_tableView->setCurrentIndex(targetCell);
                    m_tableView->edit(targetCell);
                }
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CueListView::editNextCell(bool forward) {
    QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) {
        return;
    }

    QModelIndex next = nextEditableIndex(current, forward);
    if (next.isValid()) {
        m_tableView->setCurrentIndex(next);
        m_tableView->scrollTo(next);
        m_tableView->edit(next);
    }
}

QModelIndex CueListView::nextEditableIndex(const QModelIndex& current, bool forward) const {
    if (!current.isValid())
        return QModelIndex();

    int rowCount = m_proxyModel->rowCount();
    if (rowCount == 0)
        return QModelIndex();

    int row = current.row();
    int col = current.column();

    // order of editable columns
    static const int editableColumns[] = {CueTableModel::ColNumber, CueTableModel::ColName,
                                          CueTableModel::ColType,   CueTableModel::ColGroup,
                                          CueTableModel::ColTags,   CueTableModel::ColNotes};

    static const int editableCount = sizeof(editableColumns) / sizeof(editableColumns[0]);

    // find column's position in list
    int colPos = -1;
    for (int i = 0; i < editableCount; ++i) {
        if (editableColumns[i] == col) {
            colPos = i;
            break;
        }
    }
    if (colPos < 0)
        colPos = 0;

    // move to next position
    if (forward) {
        colPos++;
        if (colPos >= editableCount) {
            colPos = 0;
            row++;
            // wrap to first row if past end
            if (row >= rowCount) {
                row = 0;
            }
        }
    } else {
        colPos--;
        if (colPos < 0) {
            colPos = editableCount - 1;
            row--;
            // wrap to last row if before start
            if (row < 0) {
                row = rowCount - 1;
            }
        }
    }

    return m_proxyModel->index(row, editableColumns[colPos]);
}

} // namespace OpenMix
