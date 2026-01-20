#include "CueListView.h"
#include "CueFilterBar.h"
#include "CueFilterProxyModel.h"
#include "CueTableModel.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "core/UndoCommands.h"

#include <QHeaderView>
#include <QVBoxLayout>

namespace OpenMix {

CueListView::CueListView(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();

    // connect playback engine for highlighting
    connect(m_app->playbackEngine(), &PlaybackEngine::currentCueChanged, this,
            &CueListView::setCurrentCueHighlight);
    connect(m_app->playbackEngine(), &PlaybackEngine::standbyCueChanged, this,
            &CueListView::setStandbyCueHighlight);

    // connect model signals for undo integration
    connect(m_model, &CueTableModel::cueReordered, this, &CueListView::onCueReordered);
}

void CueListView::setupUi() {
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
                                 QAbstractItemView::EditKeyPressed);
    m_tableView->setAlternatingRowColors(true);

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
    m_tableView->setColumnWidth(CueTableModel::ColType, 80);
    m_tableView->setColumnWidth(CueTableModel::ColFade, 60);
    m_tableView->setColumnWidth(CueTableModel::ColGroup, 100);
    m_tableView->setColumnWidth(CueTableModel::ColTags, 120);

    // connections
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CueListView::onSelectionChanged);
    connect(m_tableView, &QTableView::doubleClicked, this, &CueListView::onDoubleClicked);

    layout->addWidget(m_filterBar);
    layout->addWidget(m_tableView);
}

int CueListView::selectedCueIndex() const {
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return -1;

    // map from proxy to source
    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
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

    // create duplicate w/ next available number
    Cue duplicate = original;
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

void CueListView::onDoubleClicked(const QModelIndex& index) {
    // map to source index
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);

    // if not an editable column, emit double-click signal
    int col = sourceIndex.column();
    if (col != CueTableModel::ColGroup && col != CueTableModel::ColTags &&
        col != CueTableModel::ColNotes && col != CueTableModel::ColName) {
        emit cueDoubleClicked(sourceIndex.row());
    }
}

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

} // namespace OpenMix
