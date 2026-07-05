#pragma once

#include "protocol/MixerCapabilities.h"
#include <QObject>
#include <QPointer>
#include <QSet>
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
class CrashRecovery;
class MidiInputManager;
class ConsoleDiscoveryService;
class AppLogger;
class ConnectionLogBridge;
class OscRemoteServer;
class QLabClient;
class ReaperClient;
class CuePlayerClient;
class ScsClient;
class TimecodeTriggerList;
class ChannelMonitor;
class ScribbleController;
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
    [[nodiscard]] ReaperClient* reaperClient() { return m_reaperClient; }
    [[nodiscard]] CuePlayerClient* cuePlayerClient() { return m_cuePlayerClient; }
    [[nodiscard]] ScsClient* scsClient() { return m_scsClient; }

    // timecode triggers + channel monitor
    [[nodiscard]] TimecodeTriggerList* timecodeTriggers() { return m_timecodeTriggers; }
    [[nodiscard]] ChannelMonitor* channelMonitor() { return m_channelMonitor; }

    // scribble-strip driver (actor names, cue number, silence/clip colors)
    [[nodiscard]] ScribbleController* scribbleController() { return m_scribbleController; }

    // mixer connection
    void connectToMixer(const QString& type, const QString& host, int port);
    void connectToDiscoveredConsole(const DiscoveredConsole& console);
    void disconnectFromMixer();

    // capabilities of the connected mixer, or of the console type selected in
    // the show's mixer config when disconnected — the app-wide source of truth
    // for DCA/channel/bus counts
    [[nodiscard]] MixerCapabilities effectiveCapabilities() const;
    [[nodiscard]] int effectiveDcaCount() const { return effectiveCapabilities().dcaCount; }

    // the show's active-DCA subset (see Show::inactiveDcas)
    [[nodiscard]] QSet<int> inactiveDcas() const;
    [[nodiscard]] bool isDcaActive(int dca) const;

    // main window
    void setMainWindow(MainWindow* window);
    [[nodiscard]] MainWindow* mainWindow();

    // record-faders: while active, console fader moves are written live into the
    // current cue's channel levels.
    void setRecordFadersActive(bool active);
    [[nodiscard]] bool recordFadersActive() const { return m_recordFadersActive; }

    // initialization
    void initialize();

    // startup auto-connect
    void startupScan();

  signals:
    void mixerConnected();
    void mixerDisconnected();
    void recordFadersActiveChanged(bool active);
    void dcaCountChanged(int count);
    void activeDcasChanged();

  private:
    void setupMixerConnection(const QString& type, const QString& host, int port);
    void refreshDcaCount();

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
    ReaperClient* m_reaperClient;
    CuePlayerClient* m_cuePlayerClient;
    ScsClient* m_scsClient;

    bool m_recordFadersActive = false;

    // timecode-triggered cues + channel silence/clip monitoring
    TimecodeTriggerList* m_timecodeTriggers;
    ChannelMonitor* m_channelMonitor;

    // scribble-strip driver
    ScribbleController* m_scribbleController;

    // last dcaCountChanged emission, so sinks only rebuild on real change
    int m_lastDcaCount = -1;
};

} // namespace OpenMix
