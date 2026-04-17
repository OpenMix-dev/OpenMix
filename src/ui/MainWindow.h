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

  protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private slots:
    // file menu actions
    void newShow();
    void openShow();
    void saveShow();
    void saveShowAs();

    // edit menu actions
    void addCue();
    void deleteCue();
    void renumberCues();

    // playback actions
    void go();
    void stopPlayback();

    // view actions - pop-out windows
    void toggleConnectionPanel();
    void toggleMixerFeedbackPanel();
    void toggleDCAMappingPanel();

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
    void showKeyboardShortcutsDialog();
    void showLogViewerDialog();

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

    // pop-out windows
    PopOutWindow* m_connectionPopOut;
    PopOutWindow* m_mixerFeedbackPopOut;
    PopOutWindow* m_dcaMappingPopOut;

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
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_exitAction;

    // edit actions
    QAction* m_addCueAction;
    QAction* m_deleteCueAction;
    QAction* m_renumberAction;
    QAction* m_undoAction;
    QAction* m_redoAction;

    // playback actions
    QAction* m_goAction;
    QAction* m_stopAction;
    QAction* m_previousCueAction;
    QAction* m_nextCueAction;

    // safety actions
    QAction* m_panicAction;
    QAction* m_panicRestoreAction;

    // view actions (for menu checkable items)
    QAction* m_showConnectionAction;
    QAction* m_showMixerFeedbackAction;
    QAction* m_showDCAMappingAction;
    QAction* m_showLogViewerAction;

    // settings actions
    QAction* m_keyboardShortcutsAction;
    QAction* m_midiControllerAction;

    // help actions
    QAction* m_aboutAction;

    // status bar
    QLabel* m_connectionStatusLabel;
    QLabel* m_cueStatusLabel;
    QLabel* m_currentCueLabel;
    QLabel* m_nextCueLabel;
};

} // namespace OpenMix
