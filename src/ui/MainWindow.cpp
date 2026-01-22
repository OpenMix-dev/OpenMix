#include "MainWindow.h"
#include "ConnectionPanel.h"
#include "CueEditor.h"
#include "CueListView.h"
#include "CueTableModel.h"
#include "LiveEditPanel.h"
#include "MixerFeedbackPanel.h"
#include "TimelineView.h"
#include "app/Application.h"
#include "core/CueList.h"
#include "core/CueValidator.h"
#include "core/LiveEditSession.h"
#include "core/PlaybackEngine.h"
#include "core/PlaybackGuard.h"
#include "core/Show.h"
#include "io/ProjectFile.h"
#include "midi/MidiInputManager.h"
#include "protocol/MixerProtocol.h"
#include "ui/MidiConfigDialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>

namespace OpenMix {

MainWindow::MainWindow(Application* app, QWidget* parent) : QMainWindow(parent), m_app(app) {
    setWindowTitle("OpenMix");
    setMinimumSize(800, 600);

    setupUi();
    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    createDockWidgets();
    connectSignals();
    loadSettings();
    updateActions();
    updateTitle();
}

MainWindow::~MainWindow() { saveSettings(); }

void MainWindow::setupUi() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    m_cueListView = new CueListView(m_app, this);
    m_cueEditor = new CueEditor(m_app, this);

    m_mainSplitter->addWidget(m_cueListView);
    m_mainSplitter->addWidget(m_cueEditor);
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 1);

    m_mainSplitter->setChildrenCollapsible(false);
    m_cueListView->setMinimumWidth(200);
    m_cueEditor->setMinimumWidth(150);

    setCentralWidget(m_mainSplitter);
}

void MainWindow::createActions() {
    m_newAction = new QAction(tr("&New Show"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newShow);

    m_openAction = new QAction(tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openShow);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveShow);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveShowAs);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QMainWindow::close);

    m_addCueAction = new QAction(tr("&Add Cue"), this);
    m_addCueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(m_addCueAction, &QAction::triggered, this, &MainWindow::addCue);

    m_deleteCueAction = new QAction(tr("&Delete Cue"), this);
    m_deleteCueAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteCueAction, &QAction::triggered, this, &MainWindow::deleteCue);

    m_renumberAction = new QAction(tr("&Renumber Cues..."), this);
    connect(m_renumberAction, &QAction::triggered, this, &MainWindow::renumberCues);

    m_undoAction = m_app->undoStack()->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);

    m_redoAction = m_app->undoStack()->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);

    m_goAction = new QAction(tr("&GO"), this);
    m_goAction->setShortcut(Qt::Key_Space);
    connect(m_goAction, &QAction::triggered, this, &MainWindow::go);

    m_stopAction = new QAction(tr("&Stop"), this);
    m_stopAction->setShortcut(Qt::Key_Escape);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    m_previousCueAction = new QAction(tr("&Previous Cue"), this);
    m_previousCueAction->setShortcut(Qt::Key_Up);
    connect(m_previousCueAction, &QAction::triggered,
            [this]() { m_app->playbackEngine()->previous(); });

    m_nextCueAction = new QAction(tr("&Next Cue"), this);
    m_nextCueAction->setShortcut(Qt::Key_Down);
    connect(m_nextCueAction, &QAction::triggered, [this]() { m_app->playbackEngine()->next(); });

    m_panicAction = new QAction(tr("PANIC"), this);
    m_panicAction->setShortcut(Qt::SHIFT | Qt::Key_Escape);
    m_panicAction->setToolTip(tr("Emergency stop - fade all to safe values (Shift+Escape)"));
    connect(m_panicAction, &QAction::triggered, this, &MainWindow::panic);

    m_panicRestoreAction = new QAction(tr("Panic + Restore"), this);
    m_panicRestoreAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Escape);
    m_panicRestoreAction->setToolTip(
        tr("Panic then restore to previous state (Ctrl+Shift+Escape)"));
    connect(m_panicRestoreAction, &QAction::triggered, this, &MainWindow::panicRestore);

    m_showConnectionAction = new QAction(tr("&Connection Panel"), this);
    m_showConnectionAction->setCheckable(true);
    m_showConnectionAction->setChecked(true);
    connect(m_showConnectionAction, &QAction::triggered, this, &MainWindow::showConnectionPanel);

    m_showTimelineAction = new QAction(tr("&Timeline"), this);
    m_showTimelineAction->setCheckable(true);
    m_showTimelineAction->setChecked(true);
    connect(m_showTimelineAction, &QAction::triggered, this, &MainWindow::showTimelineView);

    m_showMixerFeedbackAction = new QAction(tr("&Mixer Feedback"), this);
    m_showMixerFeedbackAction->setCheckable(true);
    m_showMixerFeedbackAction->setChecked(false);
    connect(m_showMixerFeedbackAction, &QAction::triggered, this,
            &MainWindow::showMixerFeedbackPanel);

    m_showLiveEditAction = new QAction(tr("&Live Edit"), this);
    m_showLiveEditAction->setCheckable(true);
    m_showLiveEditAction->setChecked(false);
    m_showLiveEditAction->setShortcut(Qt::CTRL | Qt::Key_L);
    connect(m_showLiveEditAction, &QAction::triggered, this, &MainWindow::showLiveEditPanel);
}

void MainWindow::createMenus() {
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_recentProjectsMenu = m_fileMenu->addMenu(tr("Open &Recent"));
    updateRecentProjectsMenu();
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_addCueAction);
    m_editMenu->addAction(m_deleteCueAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_renumberAction);

    m_playbackMenu = menuBar()->addMenu(tr("&Playback"));
    m_playbackMenu->addAction(m_goAction);
    m_playbackMenu->addAction(m_stopAction);
    m_playbackMenu->addSeparator();
    m_playbackMenu->addAction(m_previousCueAction);
    m_playbackMenu->addAction(m_nextCueAction);
    m_playbackMenu->addSeparator();
    m_playbackMenu->addAction(m_panicAction);
    m_playbackMenu->addAction(m_panicRestoreAction);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_showConnectionAction);
    m_viewMenu->addAction(m_showTimelineAction);
    m_viewMenu->addAction(m_showMixerFeedbackAction);
    m_viewMenu->addAction(m_showLiveEditAction);

    m_settingsMenu = menuBar()->addMenu(tr("&Settings"));
    QAction* midiControllerAction = m_settingsMenu->addAction(tr("MIDI Controller..."));
    connect(midiControllerAction, &QAction::triggered, this, &MainWindow::showMidiConfigDialog);

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction = m_helpMenu->addAction(tr("&About OpenMix"));
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(
            this, tr("About OpenMix"),
            tr("<h3>OpenMix</h3>"
               "<p>Open Source Theatre Sound Mixing Control</p>"
               "<p>Version 0.1.0</p>"
               "<p>A live-sound mixing control application for theatre productions.</p>"));
    });
}

void MainWindow::createToolBars() {
    m_mainToolBar = addToolBar(tr("Main"));
    m_mainToolBar->setObjectName("MainToolBar");
    m_mainToolBar->addAction(m_newAction);
    m_mainToolBar->addAction(m_openAction);
    m_mainToolBar->addAction(m_saveAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_addCueAction);
    m_mainToolBar->addAction(m_deleteCueAction);

    m_playbackToolBar = addToolBar(tr("Playback"));
    m_playbackToolBar->setObjectName("PlaybackToolBar");
    m_playbackToolBar->addAction(m_goAction);
    m_playbackToolBar->addAction(m_stopAction);
    m_playbackToolBar->addSeparator();
    m_playbackToolBar->addAction(m_previousCueAction);
    m_playbackToolBar->addAction(m_nextCueAction);
    m_playbackToolBar->addSeparator();
    m_playbackToolBar->addAction(m_panicAction);
}

void MainWindow::createStatusBar() {
    m_connectionStatusLabel = new QLabel(tr("Disconnected"));
    m_cueStatusLabel = new QLabel(tr("No cues"));
    m_playbackStatusLabel = new QLabel(tr("Stopped"));

    statusBar()->addWidget(m_connectionStatusLabel);
    statusBar()->addWidget(m_cueStatusLabel, 1);
    statusBar()->addPermanentWidget(m_playbackStatusLabel);
}

void MainWindow::createDockWidgets() {
    m_connectionPanel = new ConnectionPanel(m_app, this);
    m_connectionDock = new QDockWidget(tr("Mixer Connection"), this);
    m_connectionDock->setObjectName("ConnectionDock");
    m_connectionDock->setWidget(m_connectionPanel);
    m_connectionDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_connectionDock);

    m_showConnectionAction->setChecked(m_connectionDock->isVisible());
    connect(m_connectionDock, &QDockWidget::visibilityChanged, m_showConnectionAction,
            &QAction::setChecked);

    m_timelineView = new TimelineView(m_app, this);
    m_timelineDock = new QDockWidget(tr("Timeline"), this);
    m_timelineDock->setObjectName("TimelineDock");
    m_timelineDock->setWidget(m_timelineView);
    m_timelineDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_timelineDock);

    m_showTimelineAction->setChecked(m_timelineDock->isVisible());
    connect(m_timelineDock, &QDockWidget::visibilityChanged, m_showTimelineAction,
            &QAction::setChecked);

    m_mixerFeedbackPanel = new MixerFeedbackPanel(m_app, this);
    m_mixerFeedbackDock = new QDockWidget(tr("Mixer Feedback"), this);
    m_mixerFeedbackDock->setObjectName("MixerFeedbackDock");
    m_mixerFeedbackDock->setWidget(m_mixerFeedbackPanel);
    m_mixerFeedbackDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea |
                                         Qt::BottomDockWidgetArea);

    addDockWidget(Qt::RightDockWidgetArea, m_mixerFeedbackDock);
    m_mixerFeedbackDock->setVisible(false); // hidden by default

    m_showMixerFeedbackAction->setChecked(m_mixerFeedbackDock->isVisible());
    connect(m_mixerFeedbackDock, &QDockWidget::visibilityChanged, m_showMixerFeedbackAction,
            &QAction::setChecked);

    // live edit panel
    m_liveEditPanel = new LiveEditPanel(m_app, this);
    m_liveEditDock = new QDockWidget(tr("Live Edit"), this);
    m_liveEditDock->setObjectName("LiveEditDock");
    m_liveEditDock->setWidget(m_liveEditPanel);
    m_liveEditDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_liveEditDock);
    m_liveEditDock->setVisible(false); // hidden by default

    m_showLiveEditAction->setChecked(m_liveEditDock->isVisible());
    connect(m_liveEditDock, &QDockWidget::visibilityChanged, m_showLiveEditAction,
            &QAction::setChecked);

    // tabify live edit dock w/ mixer feedback dock
    tabifyDockWidget(m_mixerFeedbackDock, m_liveEditDock);
}

void MainWindow::connectSignals() {
    connect(m_app->show(), &Show::modifiedChanged, this, &MainWindow::updateTitle);
    connect(m_app->show(), &Show::nameChanged, this, &MainWindow::updateTitle);
    connect(m_app->show()->cueList(), &CueList::cueAdded, this, &MainWindow::updateStatusBar);
    connect(m_app->show()->cueList(), &CueList::cueRemoved, this, &MainWindow::updateStatusBar);
    connect(m_app->show()->cueList(), &CueList::listCleared, this, &MainWindow::updateStatusBar);

    connect(m_app->playbackEngine(), &PlaybackEngine::stateChanged, this,
            &MainWindow::onPlaybackStateChanged);
    connect(m_app->playbackEngine(), &PlaybackEngine::currentCueChanged, this,
            &MainWindow::onCurrentCueChanged);
    connect(m_app->playbackEngine(), &PlaybackEngine::currentCueChanged, m_mixerFeedbackPanel,
            &MixerFeedbackPanel::onActiveCueChanged);
    connect(m_app->playbackEngine(), &PlaybackEngine::standbyCueChanged, this,
            &MainWindow::updateStatusBar);

    connect(m_cueListView, &CueListView::cueSelected, this, &MainWindow::onCueSelectionChanged);
    connect(m_cueListView, &CueListView::cueSelected, m_cueEditor, &CueEditor::setCue);
    connect(m_cueListView, &CueListView::cueSelected, m_mixerFeedbackPanel,
            &MixerFeedbackPanel::onActiveCueChanged);
    connect(m_cueListView, &CueListView::cueDoubleClicked,
            [this](int index) { m_app->playbackEngine()->executeCue(index); });

    connect(m_cueEditor, &CueEditor::cueModified, [this]() { m_cueListView->refreshCurrentCue(); });

    connect(m_app->playbackEngine(), &PlaybackEngine::goLockout, this, &MainWindow::onGoLockout);
    connect(m_app->playbackEngine(), &PlaybackEngine::cueValidationFailed, this,
            &MainWindow::onCueValidationFailed);

    PlaybackGuard* guard = m_app->playbackEngine()->guard();
    if (guard) {
        connect(guard, &PlaybackGuard::panicTriggered, this, &MainWindow::onPanicTriggered);
        connect(guard, &PlaybackGuard::lockoutStateChanged, this,
                &MainWindow::onLockoutStateChanged);
    }

    connect(m_timelineView, &TimelineView::cueClicked,
            [this](int index) { m_app->playbackEngine()->goToIndex(index); });

    // live edit session signals
    LiveEditSession* liveEditSession = m_app->playbackEngine()->liveEditSession();
    if (liveEditSession) {
        connect(liveEditSession, &LiveEditSession::sessionStarted, [this](const QString& cueId) {
            int index = m_app->show()->cueList()->indexOf(cueId);
            m_cueListView->model()->setLiveEditCueIndex(index);

            // show live edit panel when session starts
            m_liveEditDock->setVisible(true);
            m_liveEditDock->raise();

            // notify mixer feedback panel
            m_mixerFeedbackPanel->onLiveEditSessionStarted(cueId);
        });

        connect(liveEditSession, &LiveEditSession::sessionEnded, [this]() {
            m_cueListView->model()->setLiveEditCueIndex(-1);
            m_mixerFeedbackPanel->onLiveEditSessionEnded();
        });

        connect(liveEditSession, &LiveEditSession::modeChanged, [this](LiveEditMode mode) {
            m_mixerFeedbackPanel->onLiveEditModeChanged(static_cast<int>(mode));
        });
    }
}

void MainWindow::loadSettings() {
    QSettings settings("OpenMix", "OpenMix");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    // spacebar for GO (when not editing text)
    if (event->key() == Qt::Key_Space && !m_cueEditor->hasFocus()) {
        go();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::maybeSave() {
    if (!m_app->show()->isModified()) {
        return true;
    }

    QMessageBox::StandardButton ret = QMessageBox::warning(
        this, tr("OpenMix"), tr("The show has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save) {
        saveShow();
        return !m_app->show()->isModified();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::newShow() {
    if (!maybeSave())
        return;
    m_app->show()->newShow();
    m_cueEditor->setCue(-1);
    updateTitle();
    updateStatusBar();
}

void MainWindow::openShow() {
    if (!maybeSave())
        return;

    QString filePath =
        QFileDialog::getOpenFileName(this, tr("Open Show"), QString(), ProjectFile::FILE_FILTER);

    if (filePath.isEmpty())
        return;

    QString error;
    if (!ProjectFile::load(m_app->show(), filePath, &error)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to open show:\n%1").arg(error));
        return;
    }

    m_cueEditor->setCue(-1);
    updateTitle();
    updateStatusBar();
    updateRecentProjectsMenu();
}

void MainWindow::saveShow() {
    if (m_app->show()->filePath().isEmpty()) {
        saveShowAs();
        return;
    }

    QString error;
    if (!ProjectFile::save(m_app->show(), m_app->show()->filePath(), &error)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save show:\n%1").arg(error));
        return;
    }

    m_app->show()->setModified(false);
    updateTitle();
    updateRecentProjectsMenu();
}

void MainWindow::saveShowAs() {
    QString filePath =
        QFileDialog::getSaveFileName(this, tr("Save Show"), QString(), ProjectFile::FILE_FILTER);

    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(ProjectFile::FILE_EXTENSION)) {
        filePath += ProjectFile::FILE_EXTENSION;
    }

    QString error;
    if (!ProjectFile::save(m_app->show(), filePath, &error)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save show:\n%1").arg(error));
        return;
    }

    m_app->show()->setFilePath(filePath);
    m_app->show()->setModified(false);
    updateTitle();
    updateRecentProjectsMenu();
}

void MainWindow::addCue() { m_cueListView->addNewCue(); }

void MainWindow::deleteCue() { m_cueListView->deleteSelectedCue(); }

void MainWindow::renumberCues() {
    // simple renumber: 1, 2, 3, ...
    CueList* list = m_app->show()->cueList();
    for (int i = 0; i < list->count(); ++i) {
        (*list)[i].setNumber(i + 1);
        list->updateCue(i, list->at(i));
    }
    m_cueListView->refreshAll();
}

void MainWindow::go() { m_app->playbackEngine()->go(); }

void MainWindow::stopPlayback() { m_app->playbackEngine()->stop(); }

void MainWindow::showConnectionPanel(bool show) {
    if (m_connectionDock) {
        m_connectionDock->setVisible(show);
    }
}

void MainWindow::showTimelineView(bool show) {
    if (m_timelineDock) {
        m_timelineDock->setVisible(show);
    }
}

void MainWindow::showMixerFeedbackPanel(bool show) {
    if (m_mixerFeedbackDock) {
        m_mixerFeedbackDock->setVisible(show);
    }
}

void MainWindow::showLiveEditPanel(bool show) {
    if (m_liveEditDock) {
        m_liveEditDock->setVisible(show);
    }
}

void MainWindow::updateTitle() {
    QString title = m_app->show()->name();
    if (m_app->show()->isModified()) {
        title += " *";
    }
    title += " - OpenMix";
    setWindowTitle(title);
}

void MainWindow::updateActions() {
    bool hasCues = m_app->show()->cueList()->count() > 0;
    m_deleteCueAction->setEnabled(hasCues && m_cueListView->selectedCueIndex() >= 0);
    m_renumberAction->setEnabled(hasCues);
    m_goAction->setEnabled(hasCues);
}

void MainWindow::updateStatusBar() {
    int count = m_app->show()->cueList()->count();
    m_cueStatusLabel->setText(tr("%n cue(s)", "", count));

    int standbyIdx = m_app->playbackEngine()->standbyCueIndex();
    if (standbyIdx >= 0) {
        const Cue* cue = m_app->playbackEngine()->standbyCue();
        m_playbackStatusLabel->setText(
            tr("Next: %1 - %2")
                .arg(cue->number(), 0, 'f', 1)
                .arg(cue->name().isEmpty() ? tr("(unnamed)") : cue->name()));
    } else {
        m_playbackStatusLabel->setText(tr("End of list"));
    }

    updateActions();
}

void MainWindow::onPlaybackStateChanged() {
    PlaybackState state = m_app->playbackEngine()->state();
    QString stateStr;
    switch (state) {
    case PlaybackState::Stopped:
        stateStr = tr("Stopped");
        break;
    case PlaybackState::Running:
        stateStr = tr("Running");
        break;
    case PlaybackState::Fading:
        stateStr = tr("Fading");
        break;
    case PlaybackState::Paused:
        stateStr = tr("Paused");
        break;
    }
    // could update UI based on state
}

void MainWindow::onCurrentCueChanged(int index) {
    m_cueListView->setCurrentCueHighlight(index);
    updateStatusBar();
}

void MainWindow::onCueSelectionChanged() { updateActions(); }

void MainWindow::updateRecentProjectsMenu() {
    m_recentProjectsMenu->clear();
    QStringList recent = ProjectFile::recentProjects();

    if (recent.isEmpty()) {
        QAction* noRecent = m_recentProjectsMenu->addAction(tr("(No recent projects)"));
        noRecent->setEnabled(false);
    } else {
        for (const QString& path : recent) {
            QAction* action = m_recentProjectsMenu->addAction(QFileInfo(path).fileName());
            action->setData(path);
            action->setToolTip(path);
            connect(action, &QAction::triggered, [this, path]() {
                if (!maybeSave())
                    return;
                QString error;
                if (!ProjectFile::load(m_app->show(), path, &error)) {
                    QMessageBox::warning(this, tr("Error"),
                                         tr("Failed to open show:\n%1").arg(error));
                    return;
                }
                m_cueEditor->setCue(-1);
                updateTitle();
                updateStatusBar();
            });
        }

        m_recentProjectsMenu->addSeparator();
        QAction* clearAction = m_recentProjectsMenu->addAction(tr("Clear Recent"));
        connect(clearAction, &QAction::triggered, [this]() {
            ProjectFile::clearRecentProjects();
            updateRecentProjectsMenu();
        });
    }
}

void MainWindow::panic() {
    PlaybackGuard* guard = m_app->playbackEngine()->guard();
    if (guard) {
        guard->panic(0.5); // 0.5 second fade to safe state
    } else {
        // fallback: just stop playback
        m_app->playbackEngine()->stop();
    }
}

void MainWindow::panicRestore() {
    PlaybackGuard* guard = m_app->playbackEngine()->guard();
    if (guard) {
        guard->panicAndRestore(0.5);
    } else {
        m_app->playbackEngine()->stop();
    }
}

void MainWindow::onGoLockout(const QString& reason) {
    statusBar()->showMessage(tr("GO blocked: %1").arg(reason), 3000);
}

void MainWindow::onCueValidationFailed(int index) {
    const Cue* cue = nullptr;
    if (m_app->show()->cueList() && index >= 0 && index < m_app->show()->cueList()->count()) {
        cue = &m_app->show()->cueList()->at(index);
    }

    QString cueInfo = cue ? tr("Cue %1 (%2)").arg(cue->number(), 0, 'f', 1).arg(cue->name())
                          : tr("Cue at index %1").arg(index);

    QMessageBox::warning(this, tr("Cue Validation Failed"),
                         tr("%1 failed validation & cannot be executed.\n\n"
                            "Check the cue's macro references & parameters.")
                             .arg(cueInfo));
}

void MainWindow::onPanicTriggered() {
    statusBar()->showMessage(tr("PANIC - Fading to safe state"), 5000);

    // visual feedback - could flash the PANIC button or show a dialog
    m_panicAction->setEnabled(false);
    QTimer::singleShot(1000, this, [this]() { m_panicAction->setEnabled(true); });
}

void MainWindow::onLockoutStateChanged(bool locked) {
    if (locked) {
        m_goAction->setEnabled(false);
        statusBar()->showMessage(tr("GO locked during fade"), 2000);
    } else {
        m_goAction->setEnabled(true);
    }
}

void MainWindow::showMidiConfigDialog() {
    MidiConfigDialog dialog(m_app->midiInputManager(), this);
    dialog.exec();
}

} // namespace OpenMix
