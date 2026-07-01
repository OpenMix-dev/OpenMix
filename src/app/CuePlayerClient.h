#pragma once

#include <QObject>
#include <QString>
#include <lo/lo.h>

namespace OpenMix {

// Outbound link to a Cue Player (cpsound) playback device: on a linked cue's
// GO, sends /cpsound/play so the external player advances in step. Matches the
// cpsound OSC dialect (default port 50550).
class CuePlayerClient : public QObject {
    Q_OBJECT

  public:
    static constexpr int CUEPLAYER_DEFAULT_PORT = 50550;

    explicit CuePlayerClient(QObject* parent = nullptr);
    ~CuePlayerClient() override;

    void setTarget(const QString& host, int port);
    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] int port() const { return m_port; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    void loadFromSettings();
    void saveToSettings();

  public slots:
    void play(); // /cpsound/play
    void stop(); // /cpsound/stop

  signals:
    void sent(const QString& address);

  private:
    void rebuildAddress();
    void send(const QString& address);

    lo_address m_address = nullptr;
    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = CUEPLAYER_DEFAULT_PORT;
    bool m_enabled = false;
};

} // namespace OpenMix
