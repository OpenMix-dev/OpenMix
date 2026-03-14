#pragma once

#include "ui/MainWindow.h"
#include <QObject>
#include <QPointer>
#include <QUndoStack>

namespace OpenMix {

class Show;
class PlaybackEngine;
class MixerProtocol;
class AutosaveManager;
class CueValidator;
class PlaybackGuard;
class PlaybackLogger;
class DryRunEngine;
class ShortcutManager;
class OperationModeManager;
class CrashRecovery;
class MidiInputManager;
class ConsoleDiscoveryService;
class AppLogger;
class ConnectionLogBridge;
struct DiscoveredConsole;

class Application : public QObject {
    Q_OBJECT

  public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    // singleton access
    static Application* instance() { return s_instance; }

    // core components
    Show* show() { return m_show; }
    PlaybackEngine* playbackEngine() { return m_playbackEngine; }
    MixerProtocol* mixer() { return m_mixer; }
    QUndoStack* undoStack() { return m_undoStack; }
    AutosaveManager* autosaveManager() { return m_autosaveManager; }

    // safety & validation
    CueValidator* cueValidator() { return m_cueValidator; }
    PlaybackGuard* playbackGuard() { return m_playbackGuard; }
    PlaybackLogger* playbackLogger() { return m_playbackLogger; }
    DryRunEngine* dryRunEngine() { return m_dryRunEngine; }

    // operator experience
    ShortcutManager* shortcutManager() { return m_shortcutManager; }
    OperationModeManager* operationModeManager() { return m_operationModeManager; }

    // recovery
    CrashRecovery* crashRecovery() { return m_crashRecovery; }

    // MIDI input
    MidiInputManager* midiInputManager() { return m_midiInputManager; }

    // console discovery
    ConsoleDiscoveryService* discoveryService() { return m_discoveryService; }

    // application logging
    AppLogger* appLogger() { return m_appLogger; }

    // mixer connection
    void connectToMixer(const QString& type, const QString& host, int port);
    void connectToDiscoveredConsole(const DiscoveredConsole& console);
    void disconnectFromMixer();

    // main window
    void setMainWindow(MainWindow* window) { m_mainWindow = window; }
    MainWindow* mainWindow() { return m_mainWindow; }

    // initialization
    void initialize();

  signals:
    void mixerConnected();
    void mixerDisconnected();

  private:
    static Application* s_instance;

    Show* m_show;
    PlaybackEngine* m_playbackEngine;
    MixerProtocol* m_mixer = nullptr;
    QUndoStack* m_undoStack;
    AutosaveManager* m_autosaveManager;
    QPointer<MainWindow> m_mainWindow;

    // safety & validation
    CueValidator* m_cueValidator;
    PlaybackGuard* m_playbackGuard;
    PlaybackLogger* m_playbackLogger;
    DryRunEngine* m_dryRunEngine;

    // operator experience
    ShortcutManager* m_shortcutManager;
    OperationModeManager* m_operationModeManager;

    // recovery
    CrashRecovery* m_crashRecovery;

    // MIDI input
    MidiInputManager* m_midiInputManager;

    // console discovery
    ConsoleDiscoveryService* m_discoveryService;

    // application logging
    AppLogger* m_appLogger;
    ConnectionLogBridge* m_connectionLogBridge;
};

} // namespace OpenMix
