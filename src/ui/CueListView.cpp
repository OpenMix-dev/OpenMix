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
#include "protocol/MixerProtocol.h"

#include <QApplication>
#include <QFocusEvent>
#include "theme/Theme.h"

#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

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

    // column widths (ColNumber wider to fit the ▶/→ standby markers)
    m_tableView->setColumnWidth(CueTableModel::ColNumber, 78);
    m_tableView->setColumnWidth(CueTableModel::ColName, 150);
    m_tableView->setColumnWidth(CueTableModel::ColType, 100);
    m_tableView->setColumnWidth(CueTableModel::ColGroup, 100);
    m_tableView->setColumnWidth(CueTableModel::ColTags, 120);
    m_tableView->setColumnWidth(CueTableModel::ColFade, 72);

    // columns are user-resizable; remember widths across sessions
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    restoreColumnWidths();
    connect(m_tableView->horizontalHeader(), &QHeaderView::sectionResized, this,
            [this](int, int, int) { saveColumnWidths(); });

    // right-click context menu for quick edits
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this,
            &CueListView::showContextMenu);

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

    // centered hint shown over the empty table so a fresh show isn't a blank void
    m_emptyHint = new QLabel(m_tableView->viewport());
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyHint->setStyleSheet(
        QString("color: %1; font-size: 13px;").arg(Theme::Colors::TextTertiary));
    m_emptyHint->hide();

    auto refreshHint = [this]() { updateEmptyHint(); };
    connect(m_proxyModel, &QAbstractItemModel::rowsInserted, this, refreshHint);
    connect(m_proxyModel, &QAbstractItemModel::rowsRemoved, this, refreshHint);
    connect(m_proxyModel, &QAbstractItemModel::modelReset, this, refreshHint);
    connect(m_proxyModel, &QAbstractItemModel::layoutChanged, this, refreshHint);
    updateEmptyHint();
}

void CueListView::updateEmptyHint() {
    if (!m_emptyHint)
        return;
    const bool empty = m_proxyModel->rowCount() == 0;
    if (empty) {
        const bool noCuesAtAll = !m_model->cueList() || m_model->cueList()->count() == 0;
        m_emptyHint->setText(noCuesAtAll
                                 ? tr("No cues yet.\nPress the + button (Ctrl+Shift+N) to add one.")
                                 : tr("No cues match the current filter."));
        m_emptyHint->resize(m_tableView->viewport()->size());
        m_emptyHint->move(0, 0);
    }
    m_emptyHint->setVisible(empty);
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
    // remaining columns: honor the row's standby/current background instead of
    // painting a selection block over it
    m_tableView->setItemDelegateForColumn(CueTableModel::ColColor, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColDca, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColPosition, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColFx, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColFade, m_textDelegate);

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
    sm->registerAction("edit.duplicateCue", m_duplicateCueAction, QKeySequence(Qt::CTRL | Qt::Key_D));

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

    // map from proxy to source, and never hand back an out-of-range row
    QModelIndex sourceIndex = m_proxyModel->mapToSource(current);
    const int row = sourceIndex.row();
    if (row < 0 || !m_model->cueList() || row >= m_model->cueList()->count())
        return -1;
    return row;
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

void CueListView::selectSourceRow(int sourceRow) {
    if (sourceRow < 0)
        return;
    QModelIndex sourceIndex = m_model->index(sourceRow, 0);
    QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
    if (proxyIndex.isValid())
        m_tableView->selectRow(proxyIndex.row());
}

void CueListView::insertCueAt(int index, const Cue& cue) {
    CueList* cueList = m_app->show()->cueList();
    const int clamped = std::clamp(index, 0, cueList->count());
    m_app->undoStack()->push(new AddCueCommand(cueList, cue, clamped));
    cueList->insertCue(clamped, cue);
    selectSourceRow(clamped);
    m_filterBar->updateFilterOptions();
}

// midpoint number that keeps the clone in order between a cue and its successor
static double interpolatedNumber(const CueList* cueList, int idx) {
    const double a = cueList->at(idx).number();
    const double b = (idx + 1 < cueList->count()) ? cueList->at(idx + 1).number() : a + 1.0;
    return b > a ? (a + b) / 2.0 : a + 0.1;
}

void CueListView::cloneCueAfter() {
    int idx = selectedCueIndex();
    if (idx < 0)
        return;
    CueList* cueList = m_app->show()->cueList();
    Cue clone = cueList->at(idx);
    clone.regenerateId();
    clone.setNumber(interpolatedNumber(cueList, idx));
    insertCueAt(idx + 1, clone);
}

void CueListView::copySelectedCue() {
    int idx = selectedCueIndex();
    if (idx < 0)
        return;
    m_clipboard = m_app->show()->cueList()->at(idx);
}

void CueListView::pasteCue() {
    if (!m_clipboard)
        return;
    CueList* cueList = m_app->show()->cueList();
    Cue pasted = *m_clipboard;
    pasted.regenerateId();

    int idx = selectedCueIndex();
    if (idx < 0) {
        pasted.setNumber(cueList->nextCueNumber());
        m_app->undoStack()->push(new AddCueCommand(cueList, pasted));
        cueList->addCue(pasted);
        selectSourceRow(cueList->count() - 1);
        m_filterBar->updateFilterOptions();
        return;
    }
    pasted.setNumber(interpolatedNumber(cueList, idx));
    insertCueAt(idx + 1, pasted);
}

void CueListView::pasteCueMerge() {
    int idx = selectedCueIndex();
    if (!m_clipboard || idx < 0)
        return;
    CueList* cueList = m_app->show()->cueList();
    Cue oldCue = cueList->at(idx);
    Cue newCue = oldCue;
    newCue.mergeContentFrom(*m_clipboard);
    m_app->undoStack()->push(new EditCueCommand(cueList, idx, oldCue, newCue));
    cueList->updateCue(idx, newCue);
}

void CueListView::pasteCueSwap() {
    int idx = selectedCueIndex();
    if (!m_clipboard || idx < 0)
        return;
    CueList* cueList = m_app->show()->cueList();
    Cue oldCue = cueList->at(idx);
    Cue newCue = oldCue;
    Cue stored = *m_clipboard;
    newCue.swapContentWith(stored);
    m_app->undoStack()->push(new EditCueCommand(cueList, idx, oldCue, newCue));
    cueList->updateCue(idx, newCue);
    m_clipboard = stored; // clipboard keeps the content swapped out of the cue
}

void CueListView::fillDown() {
    int idx = selectedCueIndex();
    CueList* cueList = m_app->show()->cueList();
    if (idx < 0 || idx + 1 >= cueList->count())
        return;
    Cue source = cueList->at(idx);
    Cue oldCue = cueList->at(idx + 1);
    Cue newCue = oldCue;
    newCue.mergeContentFrom(source);
    m_app->undoStack()->push(new EditCueCommand(cueList, idx + 1, oldCue, newCue));
    cueList->updateCue(idx + 1, newCue);
    selectSourceRow(idx + 1);
}

void CueListView::cloneOffsets() {
    int idx = selectedCueIndex();
    CueList* cueList = m_app->show()->cueList();
    if (idx < 0 || idx + 1 >= cueList->count())
        return;
    const QMap<int, double> levels = cueList->at(idx).channelLevels();
    if (levels.isEmpty())
        return;

    Cue oldCue = cueList->at(idx + 1);
    Cue newCue = oldCue;
    for (auto it = levels.begin(); it != levels.end(); ++it)
        newCue.setChannelLevel(it.key(), it.value());
    m_app->undoStack()->push(new EditCueCommand(cueList, idx + 1, oldCue, newCue));
    cueList->updateCue(idx + 1, newCue);
    selectSourceRow(idx + 1);
}

int CueListView::recordOffsets() {
    MixerProtocol* mixer = m_app->mixer();
    const int idx = selectedCueIndex();
    CueList* cueList = m_app->show()->cueList();
    if (!mixer || !mixer->isConnected() || idx < 0)
        return 0;

    Cue oldCue = cueList->at(idx);
    Cue newCue = oldCue;
    int count = 0;
    const int channels = mixer->capabilities().inputChannels;
    for (int ch = 1; ch <= channels; ++ch) {
        if (const std::optional<double> level = mixer->readChannelFader(ch)) {
            newCue.setChannelLevel(ch, *level);
            ++count;
        }
    }
    if (count == 0)
        return 0;

    m_app->undoStack()->push(new EditCueCommand(cueList, idx, oldCue, newCue));
    cueList->updateCue(idx, newCue);
    selectSourceRow(idx);
    return count;
}

void CueListView::jumpToSelectedCue() {
    int idx = selectedCueIndex();
    if (idx < 0)
        return;
    m_app->playbackEngine()->setStandbyIndex(idx);
}

void CueListView::setRowHeight(int pixels) {
    m_tableView->verticalHeader()->setDefaultSectionSize(pixels);
}

void CueListView::setColumnVisible(int column, bool visible) {
    m_tableView->setColumnHidden(column, !visible);
}

void CueListView::setEditingLocked(bool locked) {
    m_tableView->setEditTriggers(locked ? QAbstractItemView::NoEditTriggers
                                        : QAbstractItemView::DoubleClicked |
                                              QAbstractItemView::EditKeyPressed |
                                              QAbstractItemView::AnyKeyPressed);
}

void CueListView::showContextMenu(const QPoint& pos) {
    QModelIndex at = m_tableView->indexAt(pos);
    if (at.isValid())
        m_tableView->selectRow(at.row());

    const int row = selectedCueIndex();
    QMenu menu(this);

    if (row >= 0) {
        QAction* rename = menu.addAction(tr("Rename"));
        rename->setShortcut(QKeySequence(Qt::Key_F2));
        connect(rename, &QAction::triggered, this, &CueListView::beginRenameSelected);

        QMenu* typeMenu = menu.addMenu(tr("Change Type"));
        const CueType types[] = {CueType::Snapshot, CueType::Stop, CueType::GoTo,
                                 CueType::Wait, CueType::Macro};
        for (CueType t : types) {
            QAction* a = typeMenu->addAction(cueTypeToString(t));
            connect(a, &QAction::triggered, this, [this, row, t]() {
                m_model->setData(m_model->index(row, CueTableModel::ColType),
                                 cueTypeToString(t), Qt::EditRole);
            });
        }

        menu.addSeparator();
        menu.addAction(tr("Duplicate"), this, &CueListView::duplicateSelectedCue);
        menu.addAction(tr("Copy"), this, &CueListView::copySelectedCue);
    }

    QAction* paste = menu.addAction(tr("Paste"));
    paste->setEnabled(hasClipboardCue());
    connect(paste, &QAction::triggered, this, &CueListView::pasteCue);

    if (row >= 0) {
        menu.addSeparator();
        menu.addAction(tr("Jump to Standby"), this, &CueListView::jumpToSelectedCue);
        menu.addAction(tr("Delete"), this, &CueListView::deleteSelectedCue);
    }

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

void CueListView::saveColumnWidths() {
    QSettings s;
    s.beginGroup("CueListColumns");
    for (int c = 0; c < CueTableModel::ColCount; ++c)
        s.setValue(QString::number(c), m_tableView->columnWidth(c));
    s.endGroup();
}

void CueListView::restoreColumnWidths() {
    QSettings s;
    s.beginGroup("CueListColumns");
    for (int c = 0; c < CueTableModel::ColCount; ++c) {
        const int w = s.value(QString::number(c), 0).toInt();
        if (w > 0)
            m_tableView->setColumnWidth(c, w);
    }
    s.endGroup();
}

void CueListView::beginRenameSelected() {
    const int row = selectedCueIndex();
    if (row < 0)
        return;
    QModelIndex src = m_model->index(row, CueTableModel::ColName);
    QModelIndex proxy = m_proxyModel->mapFromSource(src);
    if (proxy.isValid()) {
        m_tableView->setCurrentIndex(proxy);
        m_tableView->edit(proxy);
    }
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
    updateEmptyHint();
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
    // keep the empty-state hint sized to the viewport
    if (watched == m_tableView->viewport() && event->type() == QEvent::Resize)
        updateEmptyHint();

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

        // F2 renames the selected cue inline
        if (keyEvent->key() == Qt::Key_F2 && !isEditing) {
            beginRenameSelected();
            return true;
        }

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
    static constexpr int editableColumns[] = {CueTableModel::ColNumber, CueTableModel::ColName,
                                             CueTableModel::ColType,   CueTableModel::ColGroup,
                                             CueTableModel::ColTags,   CueTableModel::ColNotes};

    static constexpr int editableCount = sizeof(editableColumns) / sizeof(editableColumns[0]);

    // find column's position in list
    const auto* found = std::find(std::begin(editableColumns), std::end(editableColumns), col);
    int colPos = (found != std::end(editableColumns))
        ? static_cast<int>(std::distance(std::begin(editableColumns), found))
        : 0;

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
