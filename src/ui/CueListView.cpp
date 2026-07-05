#include "CueListView.h"
#include "CastTextParse.h"
#include "CueFilterBar.h"
#include "CueFilterProxyModel.h"
#include "CueItemDelegates.h"
#include "CueTableModel.h"
#include "app/Application.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/ShortcutManager.h"
#include "core/Show.h"
#include "core/UndoCommands.h"
#include "protocol/MixerProtocol.h"

#include <QApplication>
#include <QClipboard>
#include <QFocusEvent>
#include "theme/Theme.h"

#include <QCompleter>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace OpenMix {

namespace {
// narrowest a column may get when the viewport can't fit all natural widths
constexpr int MinChokedColumnWidth = 40;
} // namespace

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
    m_model->setDcaMapping(m_app->show()->dcaMapping());
    m_model->setActorLibrary(m_app->show()->actorProfileLibrary());
    m_model->setEnsembleLibrary(m_app->show()->ensembleLibrary());
    m_model->setDcaCount(m_app->effectiveDcaCount());
    connect(m_app, &Application::dcaCountChanged, this,
            [this](int count) { m_model->setDcaCount(count); });
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
                                 QAbstractItemView::SelectedClicked |
                                 QAbstractItemView::AnyKeyPressed);

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
    m_tableView->setShowGrid(false);

    // hide the per-DCA fx/pos sub-columns by default; a model reset (DCA count
    // change) recreates the columns unhidden, so re-apply afterwards
    applyDcaSubColumnVisibility();
    connect(m_proxyModel, &QAbstractItemModel::modelReset, this,
            &CueListView::applyDcaSubColumnVisibility);
    connect(m_app, &Application::activeDcasChanged, this, [this]() {
        applyDcaSubColumnVisibility();
        scheduleColumnRelayout();
    });

    // columns auto-fit their contents; leftover space is shared, and columns only
    // shrink below their natural width when the viewport can't fit them all
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_relayoutTimer = new QTimer(this);
    m_relayoutTimer->setSingleShot(true);
    m_relayoutTimer->setInterval(0);
    connect(m_relayoutTimer, &QTimer::timeout, this, &CueListView::relayoutColumns);
    connect(m_proxyModel, &QAbstractItemModel::modelReset, this,
            &CueListView::scheduleColumnRelayout);
    connect(m_proxyModel, &QAbstractItemModel::rowsInserted, this,
            &CueListView::scheduleColumnRelayout);
    connect(m_proxyModel, &QAbstractItemModel::rowsRemoved, this,
            &CueListView::scheduleColumnRelayout);
    connect(m_proxyModel, &QAbstractItemModel::layoutChanged, this,
            &CueListView::scheduleColumnRelayout);
    connect(m_proxyModel, &QAbstractItemModel::dataChanged, this,
            &CueListView::scheduleColumnRelayout);
    scheduleColumnRelayout();

    // right-click context menu for quick edits
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this,
            &CueListView::showContextMenu);

    // double-click a recall cell opens a focused assign prompt
    connect(m_tableView, &QTableView::doubleClicked, this, &CueListView::onCellDoubleClicked);

    // connections
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CueListView::onSelectionChanged);

    // update filter options when group/tags are edited; a DCA assignment edit
    // re-emits cueSelected so CueEditor and the DCA mapping panel refresh (the
    // cascade the old assign dialog triggered)
    connect(
        m_model, &CueTableModel::cueEditRequested, this,
        [this](int row, int column, const QVariant&, const QVariant&) {
            if (column == CueTableModel::ColGroup || column == CueTableModel::ColTags) {
                m_filterBar->updateFilterOptions();
            }
            if (m_model->dcaSubColumn(column) == 0)
                emit cueSelected(row);
        },
        Qt::QueuedConnection);

    // repaint DCA assignment cells when actors are renamed / roles change
    connect(m_app->show()->actorProfileLibrary(), &ActorProfileLibrary::changed,
            m_tableView->viewport(), qOverload<>(&QWidget::update));

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
    m_dcaAssignDelegate = new DCAAssignDelegate(m_app->show()->actorProfileLibrary(),
                                                m_app->show()->ensembleLibrary(), this);

    // default delegate covers the dynamic per-DCA columns (strips the selection
    // block so the row's standby/current colour shows through; only the DCA
    // assignment sub-columns are editable, so its completer editor never
    // appears elsewhere). Covers columns added by later DCA-count changes too.
    m_tableView->setItemDelegate(m_dcaAssignDelegate);

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
    m_tableView->setItemDelegateForColumn(CueTableModel::ColFx, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColScene, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColSnip, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColExternal, m_textDelegate);
    m_tableView->setItemDelegateForColumn(CueTableModel::ColFade, m_textDelegate);

    // connect tab navigation
    connect(m_numberDelegate, &CueNumberDelegate::tabNavigationRequested, this,
            &CueListView::onTabNavigationRequested);

    connect(m_typeDelegate, &CueTypeDelegate::tabNavigationRequested, this,
            &CueListView::onTabNavigationRequested);

    connect(m_textDelegate, &CueTextDelegate::tabNavigationRequested, this,
            &CueListView::onTabNavigationRequested);

    connect(m_dcaAssignDelegate, &DCAAssignDelegate::tabNavigationRequested, this,
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

// the current cell decides what copy/paste means: on a DCA assignment cell the
// shortcuts speak spreadsheet (system-clipboard TSV), everywhere else they keep
// the whole-cue semantics
void CueListView::copySmart() {
    const QModelIndex anchor = currentDcaAssignmentIndex();
    if (anchor.isValid())
        copyDcaCells(anchor);
    else
        copySelectedCue();
}

void CueListView::pasteSmart() {
    const QModelIndex anchor = currentDcaAssignmentIndex();
    if (anchor.isValid() && pasteDcaCells(anchor))
        return;
    pasteCue();
}

QModelIndex CueListView::currentDcaAssignmentIndex() const {
    const QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid())
        return {};
    const QModelIndex src = m_proxyModel->mapToSource(current);
    return m_model->dcaSubColumn(src.column()) == 0 ? current : QModelIndex();
}

void CueListView::copyDcaCells(const QModelIndex& proxyAnchor) {
    const QModelIndex src = m_proxyModel->mapToSource(proxyAnchor);
    // EditRole = the raw typed labels, so a copy round-trips to a spreadsheet
    QStringList values;
    for (int c = src.column(); c < m_model->columnCount(); ++c) {
        if (m_model->dcaSubColumn(c) != 0 || m_tableView->isColumnHidden(c))
            continue;
        values << m_model->index(src.row(), c).data(Qt::EditRole).toString();
    }
    while (!values.isEmpty() && values.last().isEmpty())
        values.removeLast();
    if (values.isEmpty())
        return; // nothing to copy; don't clobber the clipboard
    QApplication::clipboard()->setText(values.join(QLatin1Char('\t')));
}

bool CueListView::pasteDcaCells(const QModelIndex& proxyAnchor) {
    if (m_editingLocked || !proxyAnchor.isValid())
        return false;
    const QStringList values =
        CastTextParse::parseTsvRow(QApplication::clipboard()->text());
    if (values.isEmpty())
        return false;

    const QModelIndex src = m_proxyModel->mapToSource(proxyAnchor);
    CueList* cueList = m_app->show()->cueList();
    const int row = src.row();
    if (row < 0 || row >= cueList->count() || m_model->dcaSubColumn(src.column()) != 0)
        return false;

    // spread rightward over visible assignment cells; fx/pos sub-columns and
    // inactive DCAs (hidden columns) are skipped, extra values are dropped
    QList<int> targets;
    for (int c = src.column(); c < m_model->columnCount() && targets.size() < values.size(); ++c) {
        if (m_model->dcaSubColumn(c) == 0 && !m_tableView->isColumnHidden(c))
            targets << m_model->dcaOfColumn(c);
    }
    if (targets.isEmpty())
        return false;

    Cue oldCue = cueList->at(row);
    Cue newCue = oldCue;
    for (int i = 0; i < targets.size(); ++i)
        m_model->applyDcaAssignment(newCue, targets[i], values[i]);

    m_app->undoStack()->push(new EditCueCommand(cueList, row, oldCue, newCue));
    cueList->updateCue(row, newCue);
    emit cueSelected(row); // same refresh cascade a typed DCA-cell commit fires
    return true;
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
    scheduleColumnRelayout();
}

void CueListView::setDcaSubColumnsVisible(int sub, bool visible) {
    if (sub == 1)
        m_dcaFxColsVisible = visible;
    else if (sub == 2)
        m_dcaPosColsVisible = visible;
    applyDcaSubColumnVisibility();
    scheduleColumnRelayout();
}

// single visibility authority for the dynamic DCA columns: an inactive DCA's
// whole triplet is hidden, and fx/pos additionally follow their menu toggles
void CueListView::applyDcaSubColumnVisibility() {
    for (int c = CueTableModel::ColCount; c < m_model->columnCount(); ++c) {
        const int sub = m_model->dcaSubColumn(c);
        const bool active = m_app->isDcaActive(m_model->dcaOfColumn(c));
        bool visible = active;
        if (sub == 1)
            visible = active && m_dcaFxColsVisible;
        else if (sub == 2)
            visible = active && m_dcaPosColsVisible;
        m_tableView->setColumnHidden(c, !visible);
    }
}

void CueListView::setEditingLocked(bool locked) {
    m_editingLocked = locked;
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

    // DCA assignment cells get spreadsheet-style TSV copy/paste. Use the
    // clicked index, not currentIndex(): selectRow above moved the current cell
    const bool onDcaCell =
        at.isValid() && m_model->dcaSubColumn(m_proxyModel->mapToSource(at).column()) == 0;
    if (onDcaCell) {
        menu.addSeparator();
        QAction* copyDca = menu.addAction(tr("Copy DCA Assignments"));
        connect(copyDca, &QAction::triggered, this, [this, at]() { copyDcaCells(at); });
        QAction* pasteDca = menu.addAction(tr("Paste DCA Assignments"));
        pasteDca->setEnabled(!m_editingLocked &&
                             !QApplication::clipboard()->text().isEmpty());
        connect(pasteDca, &QAction::triggered, this, [this, at]() { pasteDcaCells(at); });
    }

    if (row >= 0) {
        menu.addSeparator();
        menu.addAction(tr("Jump to Standby"), this, &CueListView::jumpToSelectedCue);
        menu.addAction(tr("Delete"), this, &CueListView::deleteSelectedCue);
    }

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

void CueListView::scheduleColumnRelayout() {
    if (m_relayoutTimer)
        m_relayoutTimer->start();
}

void CueListView::relayoutColumns() {
    QHeaderView* header = m_tableView->horizontalHeader();
    const int columnCount = m_proxyModel->columnCount();
    const int available = m_tableView->viewport()->width();
    if (columnCount <= 0 || available <= 0)
        return;

    // natural width per visible column: whatever its contents and header need
    // (QTableView re-declares sizeHintForColumn protected; the base keeps it public)
    const QAbstractItemView* view = m_tableView;
    QList<int> columns;
    QList<int> natural;
    int naturalTotal = 0;
    for (int c = 0; c < columnCount; ++c) {
        if (m_tableView->isColumnHidden(c))
            continue;
        const int w = qMax(view->sizeHintForColumn(c), header->sectionSizeHint(c));
        columns.append(c);
        natural.append(w);
        naturalTotal += w;
    }
    if (columns.isEmpty() || naturalTotal <= 0)
        return;

    // scale everything to the viewport: leftover space is shared proportionally,
    // and columns only shrink below natural width when there isn't enough room
    const bool choked = naturalTotal > available;
    int used = 0;
    for (int i = 0; i < columns.size(); ++i) {
        int w = static_cast<int>(qint64(natural[i]) * available / naturalTotal);
        if (choked) {
            // keep a usable floor; past that the horizontal scrollbar takes over
            w = qMax(w, qMin(natural[i], MinChokedColumnWidth));
        } else if (i == columns.size() - 1) {
            w = available - used; // hand rounding leftovers to the last column
        }
        m_tableView->setColumnWidth(columns[i], w);
        used += w;
    }
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

void CueListView::onCellDoubleClicked(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid())
        return;
    const QModelIndex src = m_proxyModel->mapToSource(proxyIndex);
    const int row = src.row();
    const int col = src.column();
    CueList* list = m_app->show()->cueList();
    if (row < 0 || row >= list->count())
        return;
    Cue cue = list->at(row);

    auto commit = [&]() {
        list->updateCue(row, cue);
        emit cueSelected(row);
    };
    auto parseInts = [](const QString& text) {
        QList<int> out;
        for (const QString& part : text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts)) {
            bool ok = false;
            const int v = part.toInt(&ok);
            if (ok)
                out.append(v);
        }
        return out;
    };

    // per-DCA assignment cell: the double-click edit trigger opens the inline
    // completer editor (DCAAssignDelegate); nothing to do here — and without
    // this early return the default branch would pop the cue-editor window
    if (m_model->dcaSubColumn(col) == 0)
        return;

    switch (col) {
    case CueTableModel::ColScene: {
        bool ok = false;
        QStringList cur;
        for (int s : cue.scenes())
            cur << QString::number(s);
        const QString text = QInputDialog::getText(this, tr("Assign Scenes"),
                                                   tr("Scene numbers (comma separated):"),
                                                   QLineEdit::Normal, cur.join(", "), &ok);
        if (ok) {
            cue.setScenes(parseInts(text));
            commit();
        }
        return;
    }
    case CueTableModel::ColSnip: {
        bool ok = false;
        QStringList cur;
        for (int s : cue.snippets())
            cur << QString::number(s);
        const QString text = QInputDialog::getText(this, tr("Assign Snippets"),
                                                   tr("Snippet indices (comma separated):"),
                                                   QLineEdit::Normal, cur.join(", "), &ok);
        if (ok) {
            cue.setSnippets(parseInts(text));
            commit();
        }
        return;
    }
    case CueTableModel::ColExternal: {
        bool ok = false;
        const QString text =
            QInputDialog::getText(this, tr("Assign External Cue"), tr("Playback cue id:"),
                                  QLineEdit::Normal, cue.qLabCue(), &ok);
        if (ok) {
            cue.setQLabCue(text.trimmed());
            commit();
        }
        return;
    }
    default:
        // editable text columns edit in place; other cells open the full editor
        if (col != CueTableModel::ColNumber && col != CueTableModel::ColName &&
            col != CueTableModel::ColType && col != CueTableModel::ColGroup &&
            col != CueTableModel::ColTags && col != CueTableModel::ColNotes)
            emit cueDoubleClicked(row);
        return;
    }
}

void CueListView::selectAdjacentCue(int delta) {
    const int rows = m_proxyModel->rowCount();
    if (rows == 0)
        return;
    const QModelIndex current = m_tableView->currentIndex();
    const int row = current.isValid() ? qBound(0, current.row() + delta, rows - 1)
                                      : (delta > 0 ? 0 : rows - 1);
    m_tableView->selectRow(row);
    m_tableView->scrollTo(m_proxyModel->index(row, 0));
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
    // keep the empty-state hint sized to the viewport and refit columns to it
    if (watched == m_tableView->viewport() && event->type() == QEvent::Resize) {
        updateEmptyHint();
        scheduleColumnRelayout();
    }

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
                    // start at first/last visible editable column
                    const int row = current.row();
                    const QList<int> columns = editableColumns(row);
                    if (!columns.isEmpty())
                        targetCell =
                            m_proxyModel->index(row, forward ? columns.first() : columns.last());
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

QList<int> CueListView::editableColumns(int proxyRow) const {
    // visible, editable columns in display order — keeps the Tab cycle off
    // hidden columns (Type..Notes by default, per-DCA fx/pos) and includes the
    // DCA assignment cells
    QList<int> columns;
    for (int c = 0; c < m_proxyModel->columnCount(); ++c) {
        if (m_tableView->isColumnHidden(c))
            continue;
        const QModelIndex idx = m_proxyModel->index(proxyRow, c);
        if (m_proxyModel->flags(idx) & Qt::ItemIsEditable)
            columns.append(c);
    }
    return columns;
}

QModelIndex CueListView::nextEditableIndex(const QModelIndex& current, bool forward) const {
    if (!current.isValid())
        return QModelIndex();

    int rowCount = m_proxyModel->rowCount();
    if (rowCount == 0)
        return QModelIndex();

    int row = current.row();
    const int col = current.column();

    const QList<int> columns = editableColumns(row);
    if (columns.isEmpty())
        return QModelIndex();

    const int foundPos = static_cast<int>(columns.indexOf(col));
    int colPos = foundPos >= 0 ? foundPos : 0;

    // move to next position, wrapping across rows
    if (forward) {
        colPos++;
        if (colPos >= columns.size()) {
            colPos = 0;
            row++;
            if (row >= rowCount) {
                row = 0;
            }
        }
    } else {
        colPos--;
        if (colPos < 0) {
            colPos = static_cast<int>(columns.size()) - 1;
            row--;
            if (row < 0) {
                row = rowCount - 1;
            }
        }
    }

    return m_proxyModel->index(row, columns[colPos]);
}

} // namespace OpenMix
