#pragma once

#include <QObject>
#include <QString>
#include <lo/lo.h>

namespace OpenMix {

// Outbound DAW remote: fires cues in QLab (or any OSC-controllable playback app)
// when a cue carrying a linked QLab cue id is executed, with an optional pre-roll.
class QLabClient : public QObject {
    Q_OBJECT

  public:
    static constexpr int QLAB_DEFAULT_PORT = 53000;

    explicit QLabClient(QObject* parent = nullptr);
    ~QLabClient() override;

    void setTarget(const QString& host, int port);
    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] int port() const { return m_port; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    void setPreRollMs(int ms) { m_preRollMs = ms < 0 ? 0 : ms; }
    [[nodiscard]] int preRollMs() const { return m_preRollMs; }

    // optional QLab workspace id; when set, addresses are /workspace/<id>/...
    void setWorkspaceId(const QString& id) { m_workspaceId = id; }
    [[nodiscard]] QString workspaceId() const { return m_workspaceId; }

    // workspace passcode; when set, a /connect is sent (once per target) before
    // the first command so a locked QLab workspace accepts remote control.
    void setPasscode(const QString& passcode);
    [[nodiscard]] QString passcode() const { return m_passcode; }

    // when true, back() is suppressed so our transport never rewinds QLab's
    // playhead (some rigs drive QLab forward-only from the console).
    void setSuppressBack(bool suppress) { m_suppressBack = suppress; }
    [[nodiscard]] bool suppressBack() const { return m_suppressBack; }

    void loadFromSettings();
    void saveToSettings();

  public slots:
    // fire a specific QLab cue by number/id (respects the pre-roll delay)
    void triggerCue(const QString& cueId);
    // GO on the QLab workspace
    void go();
    // step the QLab playhead back one cue (no-op when suppressBack is set)
    void back();
    // transport controls on the QLab workspace
    void panic();
    void stop();
    void pause();
    void resume();

  signals:
    void sent(const QString& address);

  private:
    void rebuildAddress();
    void send(const QString& address);
    void ensureConnected(); // send /connect with the passcode once per target
    [[nodiscard]] QString prefix() const; // "" or "/workspace/<id>"

    lo_address m_address = nullptr;
    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = QLAB_DEFAULT_PORT;
    bool m_enabled = false;
    int m_preRollMs = 0;
    QString m_workspaceId;
    QString m_passcode;
    bool m_suppressBack = false;
    bool m_connected = false; // passcode already sent for the current target
};

} // namespace OpenMix
