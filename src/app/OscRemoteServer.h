#pragma once

#include <QObject>
#include <lo/lo.h>

namespace OpenMix {

class PlaybackEngine;

// Inbound OSC control surface so external apps (QLab, a stage-manager remote)
// can drive the cue stack. Listens on a UDP port via a liblo server thread and
// re-emits each command as a Qt signal marshaled onto this object's thread.
//
// Supported addresses:
//   /go, /cue/go, /ctrl/go            -> GO (fire standby cue)
//   /stop, /cue/stop, /ctrl/stop      -> STOP
//   /next                             -> standby next
//   /prev, /previous                  -> standby previous
//   /ctrl/fadeall                     -> fade everything to safe
//   /cue/goto  (int|float arg)        -> standby cue by number
class OscRemoteServer : public QObject {
    Q_OBJECT

  public:
    enum class Command { Go, Stop, Next, Prev, FadeAll, Goto };

    explicit OscRemoteServer(QObject* parent = nullptr);
    ~OscRemoteServer() override;

    // start listening. port <= 0 picks a free port (see port()). Returns false if
    // the port could not be bound.
    bool start(int port);
    void stop();
    [[nodiscard]] bool isRunning() const { return m_server != nullptr; }
    [[nodiscard]] int port() const { return m_port; }

    // wire the standard control signals to a playback engine's slots
    void setPlaybackEngine(PlaybackEngine* engine);

    void loadFromSettings();
    void saveToSettings();

    // called by the (non-Qt) liblo server thread; marshals onto this thread
    void deliver(Command command, double arg = 0.0);

  signals:
    void goRequested();
    void stopRequested();
    void nextRequested();
    void prevRequested();
    void fadeAllRequested();
    void gotoRequested(double cueNumber);
    void started(int port);
    void stopped();

  private:
    void registerMethods();

    lo_server_thread m_server = nullptr;
    int m_port = 0;
};

} // namespace OpenMix
