#pragma once

#include <QMainWindow>
#include <QPointer>

class QAction;
class QMenu;
class QToolBar;
class QLabel;
class QSplitter;
class QDockWidget;

namespace OpenMix {

class Application;
class CueListView;
class CueEditor;
class ConnectionPanel;
class TimelineView;
class MixerFeedbackPanel;
class PlaybackGuard;
struct ValidationResult;

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(Application* app, QWidget* parent = nullptr);
    ~MainWindow() override;

  protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

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

    // view actions
    void showConnectionPanel(bool show);
    void showTimelineView(bool show);
    void showMixerFeedbackPanel(bool show);

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

  private:
    void setupUi();
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
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

    // dock widget contents
    ConnectionPanel* m_connectionPanel;
    TimelineView* m_timelineView;
    MixerFeedbackPanel* m_mixerFeedbackPanel;

    // dock widgets
    QDockWidget* m_connectionDock;
    QDockWidget* m_timelineDock;
    QDockWidget* m_mixerFeedbackDock;

    // menus
    QMenu* m_fileMenu;
    QMenu* m_recentProjectsMenu;
    QMenu* m_editMenu;
    QMenu* m_playbackMenu;
    QMenu* m_viewMenu;
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

    // view actions
    QAction* m_showConnectionAction;
    QAction* m_showTimelineAction;
    QAction* m_showMixerFeedbackAction;

    // status bar
    QLabel* m_connectionStatusLabel;
    QLabel* m_cueStatusLabel;
    QLabel* m_playbackStatusLabel;
};

} // namespace OpenMix
