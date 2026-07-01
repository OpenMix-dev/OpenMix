#include "MainWindow.h"
#include "ActorSetupPanel.h"
#include "BubbleBar.h"
#include "BubbleButton.h"
#include "ConnectionPanel.h"
#include "CueEditor.h"
#include "ActiveCueInfoPanel.h"
#include "AllocateSpareDialog.h"
#include "HelpDialog.h"
#include "FxSetupDialog.h"
#include "WelcomeDialog.h"
#include "ChannelUtilizationDialog.h"
#include "MarkerNotesDialog.h"
#include "CueListView.h"
#include "CueTableModel.h"
#include "CueZeroDialog.h"
#include "DCAMappingPanel.h"
#include "EnsemblePanel.h"
#include "PositionPanel.h"
#include "TimecodePanel.h"
#include "LogViewerDialog.h"
#include "MixerFeedbackPanel.h"
#include "PopOutWindow.h"
#include "RemoteControlDialog.h"
#include "SettingsDialog.h"
#include "app/Application.h"
#include "app/QLabClient.h"
#include "core/AppLogger.h"
#include "core/CueList.h"
#include "core/CueValidator.h"
#include "core/PlaybackEngine.h"
#include "core/PlaybackGuard.h"
#include "core/ShortcutManager.h"
#include "core/Show.h"
#include "io/ProjectFile.h"
#include "io/TmixImporter.h"
#include "midi/MidiInputManager.h"
#include "protocol/MixerProtocol.h"
#include "theme/Icons.h"
#include "theme/Theme.h"
#include "ui/KeyboardShortcutsDialog.h"
#include "ui/MidiConfigDialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QActionGroup>
#include <QDialog>
#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QUndoView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>

namespace OpenMix {

MainWindow::MainWindow(Application* app, QWidget* parent) : QMainWindow(parent), m_app(app) {
    setWindowTitle("OpenMix");
    setMinimumSize(1280, 720);
    resize(1920, 1080);

    setStyleSheet(Theme::globalStylesheet());

    setupUi();
    createActions();
    registerShortcuts();
    createMenus();
    createToolBars();
    createStatusBar();
    createPopOutWindows();
    createBubbleBar();
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
    m_cueListView->setMinimumWidth(300);
    m_cueEditor->setMinimumWidth(250);

    setCentralWidget(m_mainSplitter);
}

void MainWindow::createActions() {
    m_newAction = new QAction(Icons::fileNew(), tr("&New Show"), this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setToolTip(tr("Create a new show (Ctrl+N)"));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newShow);

    m_openAction = new QAction(Icons::fileOpen(), tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setToolTip(tr("Open an existing show (Ctrl+O)"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openShow);

    m_importTmixAction = new QAction(tr("&Import Show File..."), this);
    m_importTmixAction->setToolTip(tr("Import a .tmix show file"));
    connect(m_importTmixAction, &QAction::triggered, this, &MainWindow::importTmixShow);

    m_saveAction = new QAction(Icons::fileSave(), tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setToolTip(tr("Save the current show (Ctrl+S)"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveShow);

    m_saveAsAction = new QAction(Icons::fileSaveAs(), tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setToolTip(tr("Save the show with a new name (Ctrl+Shift+S)"));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveShowAs);

    m_exportNotesAction = new QAction(tr("Export &Notes..."), this);
    m_exportNotesAction->setToolTip(tr("Export cue notes to a text file"));
    connect(m_exportNotesAction, &QAction::triggered, this, &MainWindow::exportNotes);

    m_discardNotesAction = new QAction(tr("&Discard Notes"), this);
    m_discardNotesAction->setToolTip(tr("Clear the notes from every cue"));
    connect(m_discardNotesAction, &QAction::triggered, this, &MainWindow::discardNotes);

    m_exitAction = new QAction(Icons::appExit(), tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setToolTip(tr("Exit the application"));
    connect(m_exitAction, &QAction::triggered, this, &QMainWindow::close);

    m_addCueAction = new QAction(Icons::listAdd(), tr("&Add Cue"), this);
    m_addCueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    m_addCueAction->setToolTip(tr("Add a new cue to the list (Ctrl+Shift+N)"));
    connect(m_addCueAction, &QAction::triggered, this, &MainWindow::addCue);

    m_deleteCueAction = new QAction(Icons::listRemove(), tr("&Delete Cue"), this);
    m_deleteCueAction->setShortcut(QKeySequence::Delete);
    m_deleteCueAction->setToolTip(tr("Delete the selected cue (Delete)"));
    connect(m_deleteCueAction, &QAction::triggered, this, &MainWindow::deleteCue);

    m_renumberAction = new QAction(tr("&Renumber Cues..."), this);
    m_renumberAction->setToolTip(tr("Renumber all cues sequentially"));
    connect(m_renumberAction, &QAction::triggered, this, &MainWindow::renumberCues);

    m_undoAction = m_app->undoStack()->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setIcon(Icons::editUndo());
    m_undoAction->setToolTip(tr("Undo the last action (Ctrl+Z)"));

    m_redoAction = m_app->undoStack()->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setIcon(Icons::editRedo());
    m_redoAction->setToolTip(tr("Redo the last undone action (Ctrl+Y)"));

    m_cloneCueAction = new QAction(tr("Clo&ne Cue"), this);
    m_cloneCueAction->setToolTip(tr("Clone the selected cue in place, just after it"));
    connect(m_cloneCueAction, &QAction::triggered, this, [this]() { m_cueListView->cloneCueAfter(); });

    m_cloneToEndAction = new QAction(tr("Clone Cue to &End"), this);
    m_cloneToEndAction->setToolTip(tr("Clone the selected cue to the end of the list"));
    connect(m_cloneToEndAction, &QAction::triggered, this,
            [this]() { m_cueListView->duplicateSelectedCue(); });

    m_copyCueAction = new QAction(tr("&Copy Cue"), this);
    m_copyCueAction->setShortcut(QKeySequence::Copy);
    m_copyCueAction->setToolTip(tr("Copy the selected cue (Ctrl+C)"));
    connect(m_copyCueAction, &QAction::triggered, this,
            [this]() { m_cueListView->copySelectedCue(); updateActions(); });

    m_pasteCueAction = new QAction(tr("&Paste Cue"), this);
    m_pasteCueAction->setShortcut(QKeySequence::Paste);
    m_pasteCueAction->setToolTip(tr("Paste the copied cue as a new cue (Ctrl+V)"));
    connect(m_pasteCueAction, &QAction::triggered, this, [this]() { m_cueListView->pasteCue(); });

    m_pasteMergeAction = new QAction(tr("Paste &Merge"), this);
    m_pasteMergeAction->setToolTip(tr("Merge the copied cue's content into the selected cue"));
    connect(m_pasteMergeAction, &QAction::triggered, this,
            [this]() { m_cueListView->pasteCueMerge(); });

    m_pasteSwapAction = new QAction(tr("Paste S&wap"), this);
    m_pasteSwapAction->setToolTip(tr("Exchange content between the copied cue and the selected cue"));
    connect(m_pasteSwapAction, &QAction::triggered, this,
            [this]() { m_cueListView->pasteCueSwap(); });

    m_fillDownAction = new QAction(tr("Fill &Down"), this);
    m_fillDownAction->setToolTip(tr("Copy the selected cue's content into the next cue"));
    connect(m_fillDownAction, &QAction::triggered, this, [this]() { m_cueListView->fillDown(); });

    m_cloneOffsetsAction = new QAction(tr("Cl&one Offsets"), this);
    m_cloneOffsetsAction->setToolTip(tr("Copy the selected cue's level offsets to the next cue"));
    connect(m_cloneOffsetsAction, &QAction::triggered, this,
            [this]() { m_cueListView->cloneOffsets(); });

    m_recordOffsetsAction = new QAction(tr("&Record Offsets"), this);
    m_recordOffsetsAction->setToolTip(
        tr("Capture the console's current fader levels into the selected cue"));
    connect(m_recordOffsetsAction, &QAction::triggered, this, &MainWindow::recordOffsets);

    m_jumpToSelectedAction = new QAction(tr("&Jump to Selected Cue"), this);
    m_jumpToSelectedAction->setToolTip(tr("Set the selected cue as standby without firing"));
    connect(m_jumpToSelectedAction, &QAction::triggered, this,
            [this]() { m_cueListView->jumpToSelectedCue(); });

    m_jumpAction = new QAction(tr("Ju&mp..."), this);
    m_jumpAction->setToolTip(tr("Set standby to a cue by number without firing"));
    connect(m_jumpAction, &QAction::triggered, this, &MainWindow::showJumpDialog);

    m_lockEditingAction = new QAction(tr("&Lock Editing"), this);
    m_lockEditingAction->setCheckable(true);
    m_lockEditingAction->setToolTip(tr("Prevent cue edits during a performance"));
    connect(m_lockEditingAction, &QAction::triggered, this, &MainWindow::toggleLockEditing);

    m_goAction = new QAction(Icons::mediaPlay(), tr("&GO"), this);
    m_goAction->setShortcut(Qt::Key_Space);
    m_goAction->setToolTip(tr("Execute the next cue (Space)"));
    connect(m_goAction, &QAction::triggered, this, &MainWindow::go);

    m_stopAction = new QAction(Icons::mediaStop(), tr("&Stop"), this);
    m_stopAction->setShortcut(Qt::Key_Escape);
    m_stopAction->setToolTip(tr("Stop playback (Escape)"));
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    m_previousCueAction = new QAction(Icons::mediaPrevious(), tr("&Previous Cue"), this);
    m_previousCueAction->setShortcut(Qt::Key_Up);
    m_previousCueAction->setToolTip(tr("Select the previous cue (Up)"));
    connect(m_previousCueAction, &QAction::triggered,
            [this]() { m_app->playbackEngine()->previous(); });

    m_nextCueAction = new QAction(Icons::mediaNext(), tr("&Next Cue"), this);
    m_nextCueAction->setShortcut(Qt::Key_Down);
    m_nextCueAction->setToolTip(tr("Select the next cue (Down)"));
    connect(m_nextCueAction, &QAction::triggered, [this]() { m_app->playbackEngine()->next(); });

    m_panicAction = new QAction(Icons::warning(), tr("PANIC"), this);
    m_panicAction->setShortcut(Qt::SHIFT | Qt::Key_Escape);
    m_panicAction->setToolTip(tr("Emergency stop - recall safe values (Shift+Escape)"));
    connect(m_panicAction, &QAction::triggered, this, &MainWindow::panic);

    m_panicRestoreAction = new QAction(Icons::warning(), tr("Panic + Restore"), this);
    m_panicRestoreAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Escape);
    m_panicRestoreAction->setToolTip(
        tr("Panic then restore to previous state (Ctrl+Shift+Escape)"));
    connect(m_panicRestoreAction, &QAction::triggered, this, &MainWindow::panicRestore);

    m_spareBackupAction = new QAction(tr("Allocate &Spare..."), this);
    m_spareBackupAction->setToolTip(tr("Allocate the spare-backup channel and switch to it"));
    connect(m_spareBackupAction, &QAction::triggered, this, &MainWindow::showAllocateSpareDialog);

    m_showDCAMappingAction = new QAction(Icons::sliders(), tr("&DCA Mapping"), this);
    m_showDCAMappingAction->setCheckable(true);
    m_showDCAMappingAction->setChecked(false);
    m_showDCAMappingAction->setShortcut(Qt::Key_F5);
    m_showDCAMappingAction->setToolTip(tr("Show/hide DCA mapping panel (F5)"));
    connect(m_showDCAMappingAction, &QAction::triggered, this, &MainWindow::toggleDCAMappingPanel);

    m_showMixerFeedbackAction = new QAction(Icons::audioVolume(), tr("&Mixer Feedback"), this);
    m_showMixerFeedbackAction->setCheckable(true);
    m_showMixerFeedbackAction->setChecked(false);
    m_showMixerFeedbackAction->setShortcut(Qt::Key_F6);
    m_showMixerFeedbackAction->setToolTip(tr("Show/hide mixer feedback panel (F6)"));
    connect(m_showMixerFeedbackAction, &QAction::triggered, this,
            &MainWindow::toggleMixerFeedbackPanel);

    m_showConnectionAction = new QAction(Icons::network(), tr("&Connection Panel"), this);
    m_showConnectionAction->setCheckable(true);
    m_showConnectionAction->setChecked(false);
    m_showConnectionAction->setShortcut(Qt::Key_F7);
    m_showConnectionAction->setToolTip(tr("Show/hide connection panel (F7)"));
    connect(m_showConnectionAction, &QAction::triggered, this, &MainWindow::toggleConnectionPanel);

    m_showActorSetupAction = new QAction(Icons::actorSetup(), tr("&Actor Setup"), this);
    m_showActorSetupAction->setCheckable(true);
    m_showActorSetupAction->setChecked(false);
    m_showActorSetupAction->setShortcut(Qt::Key_F9);
    m_showActorSetupAction->setToolTip(tr("Show/hide actor setup panel (F9)"));
    connect(m_showActorSetupAction, &QAction::triggered, this, &MainWindow::toggleActorSetupPanel);

    m_showEnsembleAction = new QAction(Icons::actorSetup(), tr("&Ensembles"), this);
    m_showEnsembleAction->setCheckable(true);
    m_showEnsembleAction->setChecked(false);
    m_showEnsembleAction->setShortcut(Qt::Key_F10);
    m_showEnsembleAction->setToolTip(tr("Show/hide ensembles panel (F10)"));
    connect(m_showEnsembleAction, &QAction::triggered, this, &MainWindow::toggleEnsemblePanel);

    m_showPositionAction = new QAction(Icons::sliders(), tr("&Positions"), this);
    m_showPositionAction->setCheckable(true);
    m_showPositionAction->setChecked(false);
    m_showPositionAction->setShortcut(Qt::Key_F11);
    m_showPositionAction->setToolTip(tr("Show/hide positions panel (F11)"));
    connect(m_showPositionAction, &QAction::triggered, this, &MainWindow::togglePositionPanel);

    m_showTimecodeAction = new QAction(Icons::sliders(), tr("&Timecode Triggers"), this);
    m_showTimecodeAction->setCheckable(true);
    m_showTimecodeAction->setChecked(false);
    m_showTimecodeAction->setShortcut(Qt::Key_F12);
    m_showTimecodeAction->setToolTip(tr("Show/hide timecode triggers panel (F12)"));
    connect(m_showTimecodeAction, &QAction::triggered, this, &MainWindow::toggleTimecodePanel);

    m_showActiveCueInfoAction = new QAction(tr("Active Cue &Info"), this);
    m_showActiveCueInfoAction->setCheckable(true);
    m_showActiveCueInfoAction->setChecked(false);
    m_showActiveCueInfoAction->setToolTip(tr("Show/hide the active cue info panel"));
    connect(m_showActiveCueInfoAction, &QAction::triggered, this,
            &MainWindow::toggleActiveCueInfoPanel);

    m_cueZeroAction = new QAction(tr("Cue &Zero..."), this);
    m_cueZeroAction->setToolTip(tr("Edit the base/reset state recalled before the first cue"));
    connect(m_cueZeroAction, &QAction::triggered, this, &MainWindow::showCueZeroDialog);

    m_editHistoryAction = new QAction(tr("Edit &History"), this);
    m_editHistoryAction->setToolTip(tr("Browse and step through the edit history"));
    connect(m_editHistoryAction, &QAction::triggered, this, &MainWindow::showEditHistoryDialog);

    m_exportCsvAction = new QAction(tr("&Export to CSV..."), this);
    m_exportCsvAction->setToolTip(tr("Export the cue list to a CSV file"));
    connect(m_exportCsvAction, &QAction::triggered, this, &MainWindow::exportCuesToCsv);

    m_channelUtilizationAction = new QAction(tr("Channel &Utilization"), this);
    m_channelUtilizationAction->setToolTip(tr("Show which cues use each input channel"));
    connect(m_channelUtilizationAction, &QAction::triggered, this,
            &MainWindow::showChannelUtilizationDialog);

    m_markerNotesAction = new QAction(tr("&Marker Notes..."), this);
    m_markerNotesAction->setToolTip(tr("Review, annotate, and export REAPER sound-check markers"));
    connect(m_markerNotesAction, &QAction::triggered, this, &MainWindow::showMarkerNotesDialog);

    m_showLogViewerAction = new QAction(tr("Application &Log..."), this);
    m_showLogViewerAction->setShortcut(Qt::Key_F8);
    m_showLogViewerAction->setToolTip(tr("Show application log (F8)"));
    connect(m_showLogViewerAction, &QAction::triggered, this, &MainWindow::showLogViewerDialog);

    // settings actions
    m_keyboardShortcutsAction = new QAction(tr("Keyboard Shortcuts..."), this);
    m_keyboardShortcutsAction->setToolTip(tr("Configure keyboard shortcuts"));
    connect(m_keyboardShortcutsAction, &QAction::triggered, this,
            &MainWindow::showKeyboardShortcutsDialog);

    m_midiControllerAction = new QAction(tr("MIDI Controller..."), this);
    m_midiControllerAction->setToolTip(tr("Configure MIDI controller mappings"));
    connect(m_midiControllerAction, &QAction::triggered, this, &MainWindow::showMidiConfigDialog);

    m_remoteControlAction = new QAction(Icons::remoteControl(), tr("Remote Control..."), this);
    m_remoteControlAction->setToolTip(tr("Configure MSC, inbound OSC, and QLab remote control"));
    connect(m_remoteControlAction, &QAction::triggered, this, &MainWindow::showRemoteControlDialog);

    m_appSettingsAction = new QAction(tr("Settings..."), this);
    m_appSettingsAction->setToolTip(
        tr("Console behavior, scribble highlight, channel monitor, and QLab settings"));
    connect(m_appSettingsAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    m_fxSetupAction = new QAction(tr("&FX Setup..."), this);
    m_fxSetupAction->setToolTip(tr("Manage FX units and channel assignments"));
    connect(m_fxSetupAction, &QAction::triggered, this, &MainWindow::showFxSetupDialog);

    // help actions
    m_quickStartAction = new QAction(tr("&Quick Start"), this);
    m_quickStartAction->setToolTip(tr("Get up and running in a few steps"));
    connect(m_quickStartAction, &QAction::triggered, this, &MainWindow::showQuickStart);

    m_featureGuideAction = new QAction(tr("&Feature Guide"), this);
    m_featureGuideAction->setToolTip(tr("Tour of the panels and features"));
    connect(m_featureGuideAction, &QAction::triggered, this, &MainWindow::showFeatureGuide);

    m_aboutAction = new QAction(tr("&About OpenMix"), this);
    m_aboutAction->setToolTip(tr("About this application"));
    connect(m_aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(
            this, tr("About OpenMix"),
            tr("<h3>OpenMix</h3>"
               "<p>Version 0.1.0</p>"
               "<p>Stage mixing software for live theatre that lets engineers program, "
               "automate, and run cues seamlessly across 30+ digital mixing consoles.</p>"));
    });
}

void MainWindow::registerShortcuts() {
    ShortcutManager* sm = m_app->shortcutManager();

    // file actions
    sm->registerAction("file.new", m_newAction, QKeySequence::New);
    sm->registerAction("file.open", m_openAction, QKeySequence::Open);
    sm->registerAction("file.save", m_saveAction, QKeySequence::Save);
    sm->registerAction("file.saveAs", m_saveAsAction, QKeySequence::SaveAs);
    sm->registerAction("file.exit", m_exitAction, QKeySequence::Quit);

    // edit actions
    sm->registerAction("edit.addCue", m_addCueAction,
                       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));

    sm->registerAction("edit.deleteCue", m_deleteCueAction, QKeySequence::Delete);
    sm->registerAction("edit.renumber", m_renumberAction, QKeySequence());
    sm->registerAction("edit.undo", m_undoAction, QKeySequence::Undo);
    sm->registerAction("edit.redo", m_redoAction, QKeySequence::Redo);

    // playback actions
    sm->registerAction("playback.go", m_goAction, QKeySequence(Qt::Key_Space));
    sm->registerAction("playback.stop", m_stopAction, QKeySequence(Qt::Key_Escape));
    sm->registerAction("playback.previous", m_previousCueAction, QKeySequence(Qt::Key_Up));
    sm->registerAction("playback.next", m_nextCueAction, QKeySequence(Qt::Key_Down));
    sm->registerAction("playback.panic", m_panicAction, QKeySequence(Qt::SHIFT | Qt::Key_Escape));
    sm->registerAction("playback.panicRestore", m_panicRestoreAction,
                       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Escape));

    // view actions
    sm->registerAction("view.dcaMapping", m_showDCAMappingAction, QKeySequence(Qt::Key_F5));
    sm->registerAction("view.mixerFeedback", m_showMixerFeedbackAction, QKeySequence(Qt::Key_F6));
    sm->registerAction("view.connection", m_showConnectionAction, QKeySequence(Qt::Key_F7));
    sm->registerAction("view.actorSetup", m_showActorSetupAction, QKeySequence(Qt::Key_F9));
    sm->registerAction("view.ensembles", m_showEnsembleAction, QKeySequence(Qt::Key_F10));
    sm->registerAction("view.positions", m_showPositionAction, QKeySequence(Qt::Key_F11));
    sm->registerAction("view.timecode", m_showTimecodeAction, QKeySequence(Qt::Key_F12));
    sm->registerAction("view.cueZero", m_cueZeroAction, QKeySequence());
    sm->registerAction("view.logViewer", m_showLogViewerAction, QKeySequence(Qt::Key_F8));

    // settings actions
    sm->registerAction("settings.keyboardShortcuts", m_keyboardShortcutsAction, QKeySequence());
    sm->registerAction("settings.midiController", m_midiControllerAction, QKeySequence());
    sm->registerAction("settings.remoteControl", m_remoteControlAction, QKeySequence());
    sm->registerAction("settings.appSettings", m_appSettingsAction, QKeySequence());

    // help actions
    sm->registerAction("help.about", m_aboutAction, QKeySequence());
}

void MainWindow::createMenus() {
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addAction(m_importTmixAction);
    m_recentProjectsMenu = m_fileMenu->addMenu(tr("Open &Recent"));
    updateRecentProjectsMenu();
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exportNotesAction);
    m_fileMenu->addAction(m_discardNotesAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_addCueAction);
    m_editMenu->addAction(m_deleteCueAction);
    m_editMenu->addAction(m_cloneCueAction);
    m_editMenu->addAction(m_cloneToEndAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_copyCueAction);
    m_editMenu->addAction(m_pasteCueAction);
    m_editMenu->addAction(m_pasteMergeAction);
    m_editMenu->addAction(m_pasteSwapAction);
    m_editMenu->addAction(m_fillDownAction);
    m_editMenu->addAction(m_cloneOffsetsAction);
    m_editMenu->addAction(m_recordOffsetsAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_renumberAction);
    m_editMenu->addAction(m_jumpToSelectedAction);
    m_editMenu->addAction(m_jumpAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_lockEditingAction);

    m_playbackMenu = menuBar()->addMenu(tr("&Playback"));
    m_playbackMenu->addAction(m_goAction);
    m_playbackMenu->addAction(m_stopAction);
    m_playbackMenu->addSeparator();
    m_playbackMenu->addAction(m_previousCueAction);
    m_playbackMenu->addAction(m_nextCueAction);
    m_playbackMenu->addSeparator();
    m_playbackMenu->addAction(m_panicAction);
    m_playbackMenu->addAction(m_panicRestoreAction);
    m_playbackMenu->addSeparator();
    m_playbackMenu->addAction(m_spareBackupAction);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_showDCAMappingAction);
    m_viewMenu->addAction(m_showMixerFeedbackAction);
    m_viewMenu->addAction(m_showConnectionAction);
    m_viewMenu->addAction(m_showActorSetupAction);
    m_viewMenu->addAction(m_showEnsembleAction);
    m_viewMenu->addAction(m_showPositionAction);
    m_viewMenu->addAction(m_showTimecodeAction);
    m_viewMenu->addAction(m_showActiveCueInfoAction);
    m_viewMenu->addSeparator();

    // cue-table row size
    QMenu* rowSizeMenu = m_viewMenu->addMenu(tr("&Row Size"));
    QActionGroup* rowSizeGroup = new QActionGroup(this);
    struct RowSize {
        const char* label;
        int pixels;
    };
    const RowSize rowSizes[] = {{"&Small", 22}, {"&Medium", 28}, {"&Large", 40}};
    for (const RowSize& rs : rowSizes) {
        QAction* action = rowSizeMenu->addAction(tr(rs.label));
        action->setCheckable(true);
        action->setChecked(rs.pixels == 28); // medium default
        rowSizeGroup->addAction(action);
        const int pixels = rs.pixels;
        connect(action, &QAction::triggered, this, [this, pixels]() {
            m_cueListView->setRowHeight(pixels);
        });
    }

    // optional-column visibility
    QMenu* columnsMenu = m_viewMenu->addMenu(tr("&Columns"));
    struct ColumnToggle {
        const char* label;
        int column;
        bool visible; // default visibility
    };
    const ColumnToggle columnToggles[] = {{"&Group", CueTableModel::ColGroup, true},
                                          {"&Tags", CueTableModel::ColTags, true},
                                          {"&Notes", CueTableModel::ColNotes, true},
                                          {"Colo&r", CueTableModel::ColColor, true},
                                          {"&DCAs", CueTableModel::ColDca, false},
                                          {"&Positions", CueTableModel::ColPosition, false},
                                          {"&FX", CueTableModel::ColFx, false}};
    for (const ColumnToggle& ct : columnToggles) {
        QAction* action = columnsMenu->addAction(tr(ct.label));
        action->setCheckable(true);
        action->setChecked(ct.visible);
        const int column = ct.column;
        m_cueListView->setColumnVisible(column, ct.visible); // apply default
        connect(action, &QAction::toggled, this,
                [this, column](bool on) { m_cueListView->setColumnVisible(column, on); });
    }
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_cueZeroAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_editHistoryAction);
    m_viewMenu->addAction(m_channelUtilizationAction);
    m_viewMenu->addAction(m_markerNotesAction);
    m_viewMenu->addAction(m_exportCsvAction);
    m_viewMenu->addAction(m_showLogViewerAction);

    m_settingsMenu = menuBar()->addMenu(tr("&Settings"));
    m_settingsMenu->addAction(m_keyboardShortcutsAction);
    m_settingsMenu->addAction(m_midiControllerAction);
    m_settingsMenu->addAction(m_remoteControlAction);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_fxSetupAction);
    m_settingsMenu->addAction(m_appSettingsAction);

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_quickStartAction);
    m_helpMenu->addAction(m_featureGuideAction);
    m_helpMenu->addSeparator();
    m_helpMenu->addAction(m_aboutAction);
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
    m_currentCueLabel = new QLabel();
    m_nextCueLabel = new QLabel();

    statusBar()->addWidget(m_connectionStatusLabel);
    statusBar()->addWidget(m_cueStatusLabel, 1);
    statusBar()->addPermanentWidget(m_currentCueLabel);
    statusBar()->addPermanentWidget(m_nextCueLabel);
}

void MainWindow::createPopOutWindows() {
    m_connectionPanel = new ConnectionPanel(m_app, nullptr);
    m_connectionPopOut = new PopOutWindow("connection", tr("Mixer Connection"), this);
    m_connectionPopOut->setContentWidget(m_connectionPanel);
    m_connectionPopOut->setMinimumContentSize(350, 600);

    connect(m_connectionPopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showConnectionAction->setChecked(visible);
        m_bubbleBar->setButtonActive("connection", visible);
    });

    m_mixerFeedbackPanel = new MixerFeedbackPanel(m_app, nullptr);
    m_mixerFeedbackPopOut = new PopOutWindow("mixerFeedback", tr("Mixer Feedback"), this);
    m_mixerFeedbackPopOut->setContentWidget(m_mixerFeedbackPanel);
    m_mixerFeedbackPopOut->setMinimumContentSize(400, 350);

    connect(m_mixerFeedbackPopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showMixerFeedbackAction->setChecked(visible);
        m_bubbleBar->setButtonActive("mixer", visible);
    });

    m_dcaMappingPanel = new DCAMappingPanel(m_app, nullptr);
    m_dcaMappingPopOut = new PopOutWindow("dcaMapping", tr("DCA Mapping"), this);
    m_dcaMappingPopOut->setContentWidget(m_dcaMappingPanel);
    m_dcaMappingPopOut->setMinimumContentSize(500, 450);

    connect(m_dcaMappingPopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showDCAMappingAction->setChecked(visible);
        m_bubbleBar->setButtonActive("dcaMapping", visible);
    });

    m_actorSetupPanel = new ActorSetupPanel(m_app, nullptr);
    m_actorSetupPopOut = new PopOutWindow("actorSetup", tr("Actor Setup"), this);
    m_actorSetupPopOut->setContentWidget(m_actorSetupPanel);
    m_actorSetupPopOut->setMinimumContentSize(760, 520);

    connect(m_actorSetupPopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showActorSetupAction->setChecked(visible);
        m_bubbleBar->setButtonActive("actors", visible);
    });

    m_ensemblePanel = new EnsemblePanel(m_app, nullptr);
    m_ensemblePopOut = new PopOutWindow("ensembles", tr("Ensembles"), this);
    m_ensemblePopOut->setContentWidget(m_ensemblePanel);
    m_ensemblePopOut->setMinimumContentSize(420, 480);

    connect(m_ensemblePopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showEnsembleAction->setChecked(visible);
        m_bubbleBar->setButtonActive("ensembles", visible);
    });

    m_positionPanel = new PositionPanel(m_app, nullptr);
    m_positionPopOut = new PopOutWindow("positions", tr("Positions"), this);
    m_positionPopOut->setContentWidget(m_positionPanel);
    m_positionPopOut->setMinimumContentSize(420, 480);

    connect(m_positionPopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showPositionAction->setChecked(visible);
        m_bubbleBar->setButtonActive("positions", visible);
    });

    m_timecodePanel = new TimecodePanel(m_app, nullptr);
    m_timecodePopOut = new PopOutWindow("timecode", tr("Timecode Triggers"), this);
    m_timecodePopOut->setContentWidget(m_timecodePanel);
    m_timecodePopOut->setMinimumContentSize(480, 420);

    connect(m_timecodePopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showTimecodeAction->setChecked(visible);
        m_bubbleBar->setButtonActive("timecode", visible);
    });

    m_activeCueInfoPanel = new ActiveCueInfoPanel(m_app, nullptr);
    m_activeCueInfoPopOut = new PopOutWindow("activeCueInfo", tr("Active Cue Info"), this);
    m_activeCueInfoPopOut->setContentWidget(m_activeCueInfoPanel);
    m_activeCueInfoPopOut->setMinimumContentSize(320, 400);

    connect(m_activeCueInfoPopOut, &PopOutWindow::visibilityChanged, [this](bool visible) {
        m_showActiveCueInfoAction->setChecked(visible);
        m_bubbleBar->setButtonActive("activeCueInfo", visible);
    });
}

void MainWindow::createBubbleBar() {
    m_bubbleBar = new BubbleBar(this);

    m_bubbleBar->addButton("dcaMapping", Icons::sliders(), tr("DCA Mapping (F5)"));
    m_bubbleBar->addButton("mixer", Icons::audioVolume(), tr("Mixer Feedback (F6)"));
    m_bubbleBar->addButton("connection", Icons::network(), tr("Connection (F7)"));
    m_bubbleBar->addButton("actors", Icons::actorSetup(), tr("Actor Setup (F9)"));
    m_bubbleBar->addButton("ensembles", Icons::actor(), tr("Ensembles (F10)"));
    m_bubbleBar->addButton("positions", Icons::sliders(), tr("Positions (F11)"));
    m_bubbleBar->addButton("timecode", Icons::sliders(), tr("Timecode Triggers (F12)"));
    m_bubbleBar->addButton("activeCueInfo", Icons::sliders(), tr("Active Cue Info"));

    connect(m_bubbleBar, &BubbleBar::buttonClicked, this, &MainWindow::onBubbleButtonClicked);

    m_cueEditor->addBottomWidget(m_bubbleBar);
}

void MainWindow::onBubbleButtonClicked(const QString& id, [[maybe_unused]] bool checked) {

    if (id == "dcaMapping") {
        toggleDCAMappingPanel();
    } else if (id == "mixer") {
        toggleMixerFeedbackPanel();
    } else if (id == "connection") {
        toggleConnectionPanel();
    } else if (id == "actors") {
        toggleActorSetupPanel();
    } else if (id == "ensembles") {
        toggleEnsemblePanel();
    } else if (id == "positions") {
        togglePositionPanel();
    } else if (id == "timecode") {
        toggleTimecodePanel();
    } else if (id == "activeCueInfo") {
        toggleActiveCueInfoPanel();
    }
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

    // post-fire confidence: surface cue landed/drift from verification
    connect(m_app->playbackEngine(), &PlaybackEngine::cueLanded, m_mixerFeedbackPanel,
            &MixerFeedbackPanel::onCueLanded);
    connect(m_app->playbackEngine(), &PlaybackEngine::cueDrifted, m_mixerFeedbackPanel,
            &MixerFeedbackPanel::onCueDrifted);
    connect(m_app->playbackEngine(), &PlaybackEngine::cueLanded, this,
            [this](int) { statusBar()->showMessage(tr("Cue landed - console matches"), 2500); });
    connect(m_app->playbackEngine(), &PlaybackEngine::cueDrifted, this,
            [this](int, const QStringList& paths) {
                statusBar()->showMessage(
                    tr("Cue drift on %n parameter(s) - see Mixer Feedback", "", paths.size()), 4000);
            });

    connect(m_app->playbackEngine(), &PlaybackEngine::standbyCueChanged, this,
            &MainWindow::updateStatusBar);

    connect(m_cueListView, &CueListView::cueSelected, this, &MainWindow::onCueSelectionChanged);
    connect(m_cueListView, &CueListView::cueSelected, m_cueEditor, &CueEditor::setCue);
    connect(m_cueListView, &CueListView::cueSelected, m_mixerFeedbackPanel,
            &MixerFeedbackPanel::onActiveCueChanged);
    connect(m_cueListView, &CueListView::cueSelected, this, [this](int index) {
        if (m_app->playbackEngine()->state() == PlaybackState::Stopped) {
            m_app->playbackEngine()->setStandbyIndex(index);
        }
    });
    connect(m_cueListView, &CueListView::cueSelected, this, [this](int index) {
        CueList* cueList = m_app->show()->cueList();
        if (index >= 0 && index < cueList->count()) {
            m_dcaMappingPanel->setCurrentCue(&(*cueList)[index]);
        } else {
            m_dcaMappingPanel->clearCurrentCue();
        }
    });

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
}

void MainWindow::loadSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("MainWindow");

    // restore splitter state
    QByteArray mainSplitterState = settings.value("mainSplitter").toByteArray();
    if (!mainSplitterState.isEmpty()) {
        m_mainSplitter->restoreState(mainSplitterState);
    }

    settings.endGroup();

    // load keyboard shortcuts
    m_app->shortcutManager()->loadFromSettings();
}

void MainWindow::saveSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("MainWindow");

    settings.setValue("mainSplitter", m_mainSplitter->saveState());

    settings.endGroup();

    // save keyboard shortcuts
    m_app->shortcutManager()->saveToSettings();
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

void MainWindow::resizeEvent(QResizeEvent* event) { QMainWindow::resizeEvent(event); }

bool MainWindow::maybeSave() {
    if (!m_app->show()->isModified()) {
        return true;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("OpenMix"));
    msgBox.setText(tr("The show has been modified."));
    msgBox.setInformativeText(tr("Do you want to save your changes?"));
    msgBox.setIcon(QMessageBox::Warning);

    QPushButton* saveBtn = msgBox.addButton(tr("Save"), QMessageBox::AcceptRole);
    saveBtn->setIcon(Icons::fileSave());

    QPushButton* discardBtn = msgBox.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
    discardBtn->setIcon(Icons::listRemove());

    QPushButton* cancelBtn = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    cancelBtn->setIcon(Icons::appExit());

    msgBox.setDefaultButton(saveBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == saveBtn) {
        saveShow();
        return !m_app->show()->isModified();
    } else if (msgBox.clickedButton() == cancelBtn) {
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

void MainWindow::importTmixShow() {
    if (!maybeSave())
        return;

    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Import Show File"), QString(), tr("Show Files (*.tmix *.x32tc)"));

    if (filePath.isEmpty())
        return;

    QString error;
    TmixImporter importer;
    if (!importer.import(filePath, m_app->show(), &error)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to import show:\n%1").arg(error));
        return;
    }

    m_app->show()->setFilePath(QString()); // imported: force Save As to a native file
    m_app->show()->setModified(true);
    m_cueEditor->setCue(-1);
    m_cueListView->refreshAll();
    updateTitle();
    updateStatusBar();
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

void MainWindow::showJumpDialog() {
    CueList* list = m_app->show()->cueList();
    if (list->isEmpty())
        return;
    bool ok = false;
    const double number = QInputDialog::getDouble(this, tr("Jump to Cue"), tr("Cue number:"), 1.0,
                                                  0.0, 99999.0, 2, &ok);
    if (!ok)
        return;
    if (auto idx = list->indexOfNumber(number))
        m_app->playbackEngine()->setStandbyIndex(*idx);
    else
        QMessageBox::information(this, tr("Jump to Cue"),
                                 tr("No cue numbered %1.").arg(number));
}

void MainWindow::recordOffsets() {
    if (!m_app->mixer() || !m_app->mixer()->isConnected()) {
        QMessageBox::information(this, tr("Record Offsets"), tr("Connect to a console first."));
        return;
    }
    const int count = m_cueListView->recordOffsets();
    if (count == 0) {
        QMessageBox::information(
            this, tr("Record Offsets"),
            tr("No fader values are available yet. Try again once the console has reported "
               "its state."));
    } else {
        statusBar()->showMessage(tr("Recorded %n channel offset(s)", "", count), 3000);
    }
}

void MainWindow::toggleLockEditing() {
    m_editingLocked = m_lockEditingAction->isChecked();
    m_cueListView->setEditingLocked(m_editingLocked);
    updateActions();
}

void MainWindow::go() { m_app->playbackEngine()->go(); }

void MainWindow::stopPlayback() { m_app->playbackEngine()->stop(); }

void MainWindow::showAllocateSpareDialog() {
    AllocateSpareDialog dialog(m_app, this);
    dialog.exec();
}

void MainWindow::openConnectionPanel() {
    if (!m_connectionPopOut->isVisible()) {
        m_connectionPopOut->showAndRestore();
    }
}

void MainWindow::showWelcomeDialog() {
    WelcomeDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    switch (dialog.choice()) {
    case WelcomeDialog::Choice::NewShow:
        newShow();
        break;
    case WelcomeDialog::Choice::OpenShow:
        openShow();
        break;
    case WelcomeDialog::Choice::OpenRecent: {
        if (!maybeSave())
            return;
        QString error;
        if (!ProjectFile::load(m_app->show(), dialog.recentPath(), &error)) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to open show:\n%1").arg(error));
            return;
        }
        m_cueEditor->setCue(-1);
        updateTitle();
        updateStatusBar();
        updateRecentProjectsMenu();
        break;
    }
    case WelcomeDialog::Choice::None:
        break;
    }
}

void MainWindow::toggleConnectionPanel() {
    if (m_connectionPopOut->isVisible()) {
        m_connectionPopOut->hide();
    } else {
        m_connectionPopOut->showAndRestore();
    }
}

void MainWindow::toggleMixerFeedbackPanel() {
    if (m_mixerFeedbackPopOut->isVisible()) {
        m_mixerFeedbackPopOut->hide();
    } else {
        m_mixerFeedbackPopOut->showAndRestore();
    }
}

void MainWindow::toggleDCAMappingPanel() {
    if (m_dcaMappingPopOut->isVisible()) {
        m_dcaMappingPopOut->hide();
    } else {
        m_dcaMappingPopOut->showAndRestore();
        m_dcaMappingPanel->refresh();
    }
}

void MainWindow::toggleActorSetupPanel() {
    if (m_actorSetupPopOut->isVisible()) {
        m_actorSetupPopOut->hide();
    } else {
        m_actorSetupPopOut->showAndRestore();
        m_actorSetupPanel->refresh();
    }
}

void MainWindow::toggleEnsemblePanel() {
    if (m_ensemblePopOut->isVisible()) {
        m_ensemblePopOut->hide();
    } else {
        m_ensemblePopOut->showAndRestore();
        m_ensemblePanel->refresh();
    }
}

void MainWindow::togglePositionPanel() {
    if (m_positionPopOut->isVisible()) {
        m_positionPopOut->hide();
    } else {
        m_positionPopOut->showAndRestore();
        m_positionPanel->refresh();
    }
}

void MainWindow::toggleTimecodePanel() {
    if (m_timecodePopOut->isVisible()) {
        m_timecodePopOut->hide();
    } else {
        m_timecodePopOut->showAndRestore();
        m_timecodePanel->refresh();
    }
}

void MainWindow::toggleActiveCueInfoPanel() {
    if (m_activeCueInfoPopOut->isVisible()) {
        m_activeCueInfoPopOut->hide();
    } else {
        m_activeCueInfoPopOut->showAndRestore();
        m_activeCueInfoPanel->refresh();
    }
}

void MainWindow::showCueZeroDialog() {
    CueZeroDialog dialog(m_app, this);
    dialog.exec();
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
    bool hasSelection = m_cueListView->selectedCueIndex() >= 0;
    bool editable = !m_editingLocked;
    m_goAction->setEnabled(hasCues);

    // editing actions gate on selection and the editing lock
    m_addCueAction->setEnabled(editable);
    m_deleteCueAction->setEnabled(editable && hasCues && hasSelection);
    m_renumberAction->setEnabled(editable && hasCues);
    m_cloneCueAction->setEnabled(editable && hasSelection);
    m_cloneToEndAction->setEnabled(editable && hasSelection);
    m_copyCueAction->setEnabled(hasSelection);
    m_pasteCueAction->setEnabled(editable && m_cueListView->hasClipboardCue());
    m_pasteMergeAction->setEnabled(editable && hasSelection && m_cueListView->hasClipboardCue());
    m_pasteSwapAction->setEnabled(editable && hasSelection && m_cueListView->hasClipboardCue());
    m_fillDownAction->setEnabled(editable && hasSelection);
    m_cloneOffsetsAction->setEnabled(editable && hasSelection);
    m_recordOffsetsAction->setEnabled(editable && hasSelection);
    m_jumpToSelectedAction->setEnabled(hasSelection);
    m_jumpAction->setEnabled(hasCues);
}

void MainWindow::updateStatusBar() {
    int count = m_app->show()->cueList()->count();
    m_cueStatusLabel->setText(tr("%n cue(s)", "", count));

    int currentIdx = m_app->playbackEngine()->currentCueIndex();
    if (currentIdx >= 0) {
        const Cue* cue = m_app->playbackEngine()->currentCue();
        m_currentCueLabel->setText(tr("Current: %1 - %2")
                                       .arg(cue->number(), 0, 'f', 1)
                                       .arg(cue->name().isEmpty() ? tr("(unnamed)") : cue->name()));
    } else {
        m_currentCueLabel->setText(tr("Current: --"));
    }

    int standbyIdx = m_app->playbackEngine()->standbyCueIndex();
    if (standbyIdx >= 0) {
        const Cue* cue = m_app->playbackEngine()->standbyCue();
        m_nextCueLabel->setText(tr("Next: %1 - %2")
                                    .arg(cue->number(), 0, 'f', 1)
                                    .arg(cue->name().isEmpty() ? tr("(unnamed)") : cue->name()));
    } else {
        m_nextCueLabel->setText(tr("Next: --"));
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
        guard->panic(); // instant recall to safe state
    } else {
        // fallback: just stop playback
        m_app->playbackEngine()->stop();
    }
    // also panic a linked QLab workspace so external playback stops with us
    if (m_app->qLabClient() && m_app->qLabClient()->isEnabled())
        m_app->qLabClient()->panic();
}

void MainWindow::panicRestore() {
    PlaybackGuard* guard = m_app->playbackEngine()->guard();
    if (guard) {
        guard->panic();
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
        statusBar()->showMessage(tr("GO locked - please wait"), 2000);
    } else {
        m_goAction->setEnabled(true);
    }
}

void MainWindow::showMidiConfigDialog() {
    MidiConfigDialog dialog(m_app->midiInputManager(), this);
    dialog.exec();
}

void MainWindow::showRemoteControlDialog() {
    RemoteControlDialog dialog(m_app, this);
    dialog.exec();
}

void MainWindow::showSettingsDialog() {
    SettingsDialog dialog(m_app, this);
    dialog.exec();
}

void MainWindow::showKeyboardShortcutsDialog() {
    KeyboardShortcutsDialog dialog(m_app->shortcutManager(), this);
    dialog.exec();
}

void MainWindow::showLogViewerDialog() {
    LogViewerDialog dialog(m_app->appLogger(), this);
    dialog.exec();
}

void MainWindow::showQuickStart() {
    const QString html = tr(
        "<h2>Quick Start</h2>"
        "<ol>"
        "<li><b>Connect your console.</b> Open <i>View &rarr; Connection</i> (F7). Scan for an "
        "OSC-capable console, or pick the protocol and enter its IP and port, then Connect.</li>"
        "<li><b>Set up your cast.</b> In <i>View &rarr; Actor Setup</i> (F9) add actors, assign "
        "each to a channel, and store their voice (gain, HPF, EQ, dynamics) plus a backup "
        "voice for a spare mic.</li>"
        "<li><b>Build cues.</b> <i>Edit &rarr; Add Cue</i>, then use the Cue Editor: DCA "
        "targeting and labels, per-channel profiles, levels and positions, fade time and "
        "curve, FX mutes and console snippets.</li>"
        "<li><b>Run the show.</b> Press <b>Space</b> to GO. <b>Up/Down</b> move the standby cue "
        "without firing; <i>Edit &rarr; Jump to Selected Cue</i> sets standby to any cue.</li>"
        "<li><b>Stay safe.</b> <b>Shift+Esc</b> panics to the Cue Zero safe values. "
        "<i>Edit &rarr; Lock Editing</i> prevents accidental edits during a performance.</li>"
        "</ol>"
        "<p>Already have a show file? <i>File &rarr; Import Show File&hellip;</i> reads a "
        "<tt>.tmix</tt> database directly.</p>");
    HelpDialog dialog(tr("Quick Start"), html, this);
    dialog.exec();
}

void MainWindow::showFeatureGuide() {
    const QString html = tr(
        "<h2>Feature Guide</h2>"
        "<h3>Cues</h3>"
        "<p>The cue list is the spine of the show. The Cue Editor edits everything a cue does; "
        "the Edit menu adds clone, copy / paste / paste-merge / paste-swap, fill down, clone "
        "offsets, jump and renumber.</p>"
        "<h3>Cast &amp; voices</h3>"
        "<p>Actor Setup (F9) holds per-actor voice profiles with a backup copy. Ensembles "
        "(F10) group channels onto a shared profile slot. Spare Backup "
        "(<i>Playback &rarr; Allocate Spare&hellip;</i>) covers a failed mic and switches its "
        "backup voice to a reserved channel, now or on the next cue.</p>"
        "<h3>Console control</h3>"
        "<p>DCA Mapping (F5) assigns channels and buses to DCAs. Positions (F11) store "
        "pan / delay presets a cue can recall per channel. FX Setup names FX units and their "
        "channel sends. Cue Zero holds the base/reset state.</p>"
        "<h3>External control</h3>"
        "<p>Settings &rarr; Remote Control covers MIDI Show Control, inbound OSC, and outbound "
        "QLab/DAW with pre-roll. Timecode Triggers (F12) fire cues at an incoming timecode.</p>"
        "<h3>Monitoring &amp; reporting</h3>"
        "<p>Mixer Feedback (F6) and Active Cue Info track live state. Channel Utilization shows "
        "which cues touch each channel; Export to CSV and Export Notes produce paperwork; Edit "
        "History browses the undo stack.</p>");
    HelpDialog dialog(tr("Feature Guide"), html, this);
    dialog.exec();
}

void MainWindow::showFxSetupDialog() {
    FxSetupDialog dialog(m_app, this);
    dialog.exec();
}

void MainWindow::showEditHistoryDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit History"));
    dialog.resize(360, 480);
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QUndoView* view = new QUndoView(m_app->undoStack(), &dialog);
    view->setEmptyLabel(tr("<clean>"));
    layout->addWidget(view);
    dialog.exec();
}

void MainWindow::showMarkerNotesDialog() {
    MarkerNotesDialog dialog(m_app, this);
    dialog.exec();
}

void MainWindow::showChannelUtilizationDialog() {
    ChannelUtilizationDialog dialog(m_app, this);
    dialog.exec();
}

void MainWindow::discardNotes() {
    if (QMessageBox::question(this, tr("Discard Notes"),
                              tr("Clear the notes from every cue? This cannot be undone.")) !=
        QMessageBox::Yes)
        return;

    CueList* list = m_app->show()->cueList();
    for (int i = 0; i < list->count(); ++i) {
        if (list->at(i).notes().isEmpty())
            continue;
        Cue cue = list->at(i);
        cue.setNotes(QString());
        list->updateCue(i, cue);
    }
    m_cueEditor->setCue(m_cueListView->selectedCueIndex());
}

void MainWindow::exportNotes() {
    QString filePath = QFileDialog::getSaveFileName(this, tr("Export Notes"), QString(),
                                                    tr("Text Files (*.txt)"));
    if (filePath.isEmpty())
        return;
    if (!filePath.endsWith(".txt", Qt::CaseInsensitive))
        filePath += ".txt";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not write %1").arg(filePath));
        return;
    }

    QTextStream out(&file);
    out << m_app->show()->name() << " - Cue Notes\n\n";
    const CueList* list = m_app->show()->cueList();
    for (int i = 0; i < list->count(); ++i) {
        const Cue& cue = list->at(i);
        if (cue.notes().isEmpty())
            continue;
        out << QString::number(cue.number(), 'f', 2);
        if (!cue.name().isEmpty())
            out << " - " << cue.name();
        out << '\n' << cue.notes() << "\n\n";
    }
    file.close();
}

void MainWindow::exportCuesToCsv() {
    QString filePath = QFileDialog::getSaveFileName(this, tr("Export to CSV"), QString(),
                                                    tr("CSV Files (*.csv)"));
    if (filePath.isEmpty())
        return;
    if (!filePath.endsWith(".csv", Qt::CaseInsensitive))
        filePath += ".csv";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not write %1").arg(filePath));
        return;
    }

    // quote a field and escape embedded quotes per RFC 4180
    auto csv = [](const QString& s) {
        QString v = s;
        v.replace('"', "\"\"");
        return QString("\"%1\"").arg(v);
    };

    QTextStream out(&file);
    out << "Number,Name,Type,Notes,Color,Skip\n";
    const CueList* list = m_app->show()->cueList();
    for (int i = 0; i < list->count(); ++i) {
        const Cue& cue = list->at(i);
        out << QString::number(cue.number(), 'f', 2) << ',' << csv(cue.name()) << ','
            << csv(cueTypeToString(cue.type())) << ',' << csv(cue.notes()) << ','
            << csv(cue.color()) << ',' << (cue.skip() ? "yes" : "no") << '\n';
    }
    file.close();
}

} // namespace OpenMix
