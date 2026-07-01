#pragma once

#include <QObject>
#include <QString>
#include <lo/lo.h>

namespace OpenMix {

// Outbound link to Show Cue System (SCS) playback software over OSC (default
// UDP port 58968). On a linked cue's GO, fires the SCS master GO (/ctrl/go) so
// the external player advances in step; also exposes Stop/Fade/Pause-Resume and
// per-cue fire (/cue/go with a cue id). Each control message carries a trailing
// string argument used as an optional control password (empty when unset).
class ScsClient : public QObject {
    Q_OBJECT

  public:
    static constexpr int SCS_DEFAULT_PORT = 58968;

    explicit ScsClient(QObject* parent = nullptr);
    ~ScsClient() override;

    void setTarget(const QString& host, int port);
    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] int port() const { return m_port; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    void setPassword(const QString& password) { m_password = password; }
    [[nodiscard]] QString password() const { return m_password; }

    void loadFromSettings();
    void saveToSettings();

  public slots:
    void go();          // /ctrl/go
    void stop();        // /ctrl/stopall
    void fade();        // /ctrl/fadeall
    void pauseResume(); // /ctrl/pauseresumeall
    void fireCue(const QString& cueId); // /cue/go <cueId>

  signals:
    void sent(const QString& address);

  private:
    void rebuildAddress();
    void send(const QString& address);

    lo_address m_address = nullptr;
    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = SCS_DEFAULT_PORT;
    bool m_enabled = false;
    QString m_password;
};

} // namespace OpenMix
