#pragma once

#include <QObject>
#include <QPointer>
#include <QUndoStack>

namespace OpenMix {

class MainWindow;
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
class OscRemoteServer;
class QLabClient;
class TimecodeTriggerList;
class ChannelMonitor;
struct DiscoveredConsole;

class Application : public QObject {
    Q_OBJECT

  public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    // singleton access
    [[nodiscard]] static Application* instance() { return s_instance; }

    // core components
    [[nodiscard]] Show* show() { return m_show; }
    [[nodiscard]] PlaybackEngine* playbackEngine() { return m_playbackEngine; }
    [[nodiscard]] MixerProtocol* mixer() { return m_mixer; }
    [[nodiscard]] QUndoStack* undoStack() { return m_undoStack; }
    [[nodiscard]] AutosaveManager* autosaveManager() { return m_autosaveManager; }

    // safety & validation
    [[nodiscard]] CueValidator* cueValidator() { return m_cueValidator; }
    [[nodiscard]] PlaybackGuard* playbackGuard() { return m_playbackGuard; }
    [[nodiscard]] PlaybackLogger* playbackLogger() { return m_playbackLogger; }
    [[nodiscard]] DryRunEngine* dryRunEngine() { return m_dryRunEngine; }

    // operator experience
    [[nodiscard]] ShortcutManager* shortcutManager() { return m_shortcutManager; }
    [[nodiscard]] OperationModeManager* operationModeManager() { return m_operationModeManager; }

    // recovery
    [[nodiscard]] CrashRecovery* crashRecovery() { return m_crashRecovery; }

    // MIDI input
    [[nodiscard]] MidiInputManager* midiInputManager() { return m_midiInputManager; }

    // console discovery
    [[nodiscard]] ConsoleDiscoveryService* discoveryService() { return m_discoveryService; }

    // application logging
    [[nodiscard]] AppLogger* appLogger() { return m_appLogger; }

    // inbound OSC remote control (QLab / stage-manager)
    [[nodiscard]] OscRemoteServer* oscRemoteServer() { return m_oscRemoteServer; }

    // outbound QLab / DAW remote
    [[nodiscard]] QLabClient* qLabClient() { return m_qLabClient; }

    // Phase 5 services
    [[nodiscard]] TimecodeTriggerList* timecodeTriggers() { return m_timecodeTriggers; }
    [[nodiscard]] ChannelMonitor* channelMonitor() { return m_channelMonitor; }

    // mixer connection
    void connectToMixer(const QString& type, const QString& host, int port);
    void connectToDiscoveredConsole(const DiscoveredConsole& console);
    void disconnectFromMixer();

    // main window
    void setMainWindow(MainWindow* window);
    [[nodiscard]] MainWindow* mainWindow();

    // initialization
    void initialize();

    // startup auto-connect
    void startupScan();

  signals:
    void mixerConnected();
    void mixerDisconnected();

  private:
    void setupMixerConnection(const QString& type, const QString& host, int port);

    static Application* s_instance;

    // All pointer members below are owned by Qt's parent-child system (parent = this).
    // m_mixer is the exception: created by ProtocolFactory with this as parent, swapped on reconnect.
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

    // inbound OSC remote control
    OscRemoteServer* m_oscRemoteServer;

    // outbound QLab / DAW remote
    QLabClient* m_qLabClient;

    // Phase 5: timecode-triggered cues + channel silence/clip monitoring
    TimecodeTriggerList* m_timecodeTriggers;
    ChannelMonitor* m_channelMonitor;
};

} // namespace OpenMix
