#pragma once

#include <QMainWindow>
#include <QPointer>

class QAction;
class QMenu;
class QToolBar;
class QLabel;
class QSplitter;

namespace OpenMix {

class Application;
class CueListView;
class CueEditor;
class ConnectionPanel;
class MixerFeedbackPanel;
class DCAMappingPanel;
class ActorSetupPanel;
class EnsemblePanel;
class PositionPanel;
class TimecodePanel;
class ActiveCueInfoPanel;
class PopOutWindow;
class BubbleBar;
class PlaybackGuard;
struct ValidationResult;

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(Application* app, QWidget* parent = nullptr);
    ~MainWindow() override;

    void openConnectionPanel();
    void showWelcomeDialog();

  protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private slots:
    // file menu actions
    void newShow();
    void openShow();
    void importTmixShow();
    void saveShow();
    void saveShowAs();
    void exportNotes();

    // edit menu actions
    void addCue();
    void deleteCue();
    void renumberCues();
    void showJumpDialog();
    void toggleLockEditing();

    // playback actions
    void go();
    void stopPlayback();
    void showAllocateSpareDialog();

    // view actions - pop-out windows
    void toggleConnectionPanel();
    void toggleMixerFeedbackPanel();
    void toggleDCAMappingPanel();
    void toggleActorSetupPanel();
    void toggleEnsemblePanel();
    void togglePositionPanel();
    void toggleTimecodePanel();
    void toggleActiveCueInfoPanel();
    void showCueZeroDialog();

    // update UI state
    void updateTitle();
    void updateActions();
    void updateStatusBar();
    void onPlaybackStateChanged();
    void onCurrentCueChanged(int index);
    void onCueSelectionChanged();

    // safety handlers
    void panic();
    void panicRestore();
    void onGoLockout(const QString& reason);
    void onCueValidationFailed(int index);
    void onPanicTriggered();
    void onLockoutStateChanged(bool locked);

    // settings dialogs
    void showMidiConfigDialog();
    void showRemoteControlDialog();
    void showKeyboardShortcutsDialog();
    void showSettingsDialog();
    void showFxSetupDialog();
    void showLogViewerDialog();
    void showEditHistoryDialog();
    void exportCuesToCsv();
    void showChannelUtilisationDialog();

    // bubble bar interaction
    void onBubbleButtonClicked(const QString& id, bool checked);

  private:
    void setupUi();
    void createActions();
    void registerShortcuts();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void createPopOutWindows();
    void createBubbleBar();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    bool maybeSave();
    void updateRecentProjectsMenu();

    Application* m_app;

    // central widgets
    QSplitter* m_mainSplitter;
    CueListView* m_cueListView;
    CueEditor* m_cueEditor;

    // pop-out window contents (owned by pop-out windows)
    ConnectionPanel* m_connectionPanel;
    MixerFeedbackPanel* m_mixerFeedbackPanel;
    DCAMappingPanel* m_dcaMappingPanel;
    ActorSetupPanel* m_actorSetupPanel;
    EnsemblePanel* m_ensemblePanel;
    PositionPanel* m_positionPanel;
    TimecodePanel* m_timecodePanel;
    ActiveCueInfoPanel* m_activeCueInfoPanel;

    // pop-out windows
    PopOutWindow* m_connectionPopOut;
    PopOutWindow* m_mixerFeedbackPopOut;
    PopOutWindow* m_dcaMappingPopOut;
    PopOutWindow* m_actorSetupPopOut;
    PopOutWindow* m_ensemblePopOut;
    PopOutWindow* m_positionPopOut;
    PopOutWindow* m_timecodePopOut;
    PopOutWindow* m_activeCueInfoPopOut;

    // bubble bar
    BubbleBar* m_bubbleBar;

    // menus
    QMenu* m_fileMenu;
    QMenu* m_recentProjectsMenu;
    QMenu* m_editMenu;
    QMenu* m_playbackMenu;
    QMenu* m_viewMenu;
    QMenu* m_settingsMenu;
    QMenu* m_helpMenu;

    // toolbars
    QToolBar* m_mainToolBar;
    QToolBar* m_playbackToolBar;

    // file actions
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_importTmixAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_exportNotesAction;
    QAction* m_exitAction;

    // edit actions
    QAction* m_addCueAction;
    QAction* m_deleteCueAction;
    QAction* m_renumberAction;
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cloneCueAction;
    QAction* m_cloneToEndAction;
    QAction* m_copyCueAction;
    QAction* m_pasteCueAction;
    QAction* m_pasteMergeAction;
    QAction* m_pasteSwapAction;
    QAction* m_fillDownAction;
    QAction* m_cloneOffsetsAction;
    QAction* m_jumpToSelectedAction;
    QAction* m_jumpAction;
    QAction* m_lockEditingAction;
    bool m_editingLocked = false;

    // playback actions
    QAction* m_goAction;
    QAction* m_stopAction;
    QAction* m_previousCueAction;
    QAction* m_nextCueAction;

    // safety actions
    QAction* m_panicAction;
    QAction* m_panicRestoreAction;
    QAction* m_spareBackupAction;

    // view actions (for menu checkable items)
    QAction* m_showConnectionAction;
    QAction* m_showMixerFeedbackAction;
    QAction* m_showDCAMappingAction;
    QAction* m_showActorSetupAction;
    QAction* m_showEnsembleAction;
    QAction* m_showPositionAction;
    QAction* m_showTimecodeAction;
    QAction* m_showActiveCueInfoAction;
    QAction* m_cueZeroAction;
    QAction* m_editHistoryAction;
    QAction* m_exportCsvAction;
    QAction* m_channelUtilisationAction;
    QAction* m_showLogViewerAction;

    // settings actions
    QAction* m_keyboardShortcutsAction;
    QAction* m_midiControllerAction;
    QAction* m_remoteControlAction;
    QAction* m_appSettingsAction;
    QAction* m_fxSetupAction;

    // help actions
    QAction* m_aboutAction;

    // status bar
    QLabel* m_connectionStatusLabel;
    QLabel* m_cueStatusLabel;
    QLabel* m_currentCueLabel;
    QLabel* m_nextCueLabel;
};

} // namespace OpenMix
