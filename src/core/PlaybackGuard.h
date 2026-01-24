#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace OpenMix {

class PlaybackEngine;
class MixerProtocol;

class PlaybackGuard : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackGuard(PlaybackEngine* engine, QObject* parent = nullptr);

    void setMixer(MixerProtocol* mixer);

    void setLockoutEnabled(bool enabled);
    bool isLockoutEnabled() const { return m_lockoutEnabled; }

    bool isGoLocked() const;
    QString lockoutReason() const;

    void setDefaultSafeValues(const QJsonObject& values);
    QJsonObject defaultSafeValues() const { return m_defaultSafeValues; }

    void panic();

    bool isPanicActive() const { return m_panicActive; }

  signals:
    void lockoutStateChanged(bool locked);
    void panicTriggered();
    void panicCompleted();

  private:
    void sendSafeValues();

    PlaybackEngine* m_engine;
    MixerProtocol* m_mixer = nullptr;

    bool m_lockoutEnabled = true;
    bool m_wasLocked = false;

    QJsonObject m_defaultSafeValues;

    bool m_panicActive = false;
};

} // namespace OpenMix
