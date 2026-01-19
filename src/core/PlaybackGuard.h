#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace StageBlend {

class PlaybackEngine;
class MixerProtocol;

struct SafeState {
    QJsonObject parameters;
    qint64 timestamp;
    QString sourceDescription;
};

class PlaybackGuard : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackGuard(PlaybackEngine* engine, QObject* parent = nullptr);

    // configure the mixer protocol for panic operations
    void setMixer(MixerProtocol* mixer);

    // gO lockout during critical fades
    void setLockoutEnabled(bool enabled);
    bool isLockoutEnabled() const { return m_lockoutEnabled; }

    // threshold: lockout GO when fade is below this progress (default 10%)
    void setLockoutThreshold(double progressPercent);
    double lockoutThreshold() const { return m_lockoutThreshold; }

    // check if GO is currently locked
    bool isGoLocked() const;
    QString lockoutReason() const;

    // safe state capture & management
    void captureSafeState(const QString& description);
    void clearSafeState();
    bool hasSafeState() const { return !m_safeState.parameters.isEmpty(); }
    SafeState safeState() const { return m_safeState; }

    // define default safe values (faders→0, mutes→on)
    void setDefaultSafeValues(const QJsonObject& values);
    QJsonObject defaultSafeValues() const { return m_defaultSafeValues; }

    // emergency operations
    void panic(double fadeOutSeconds = 0.5);     // fade to safe state
    void panicImmediate();                       // instant snap to safe state
    void panicAndRestore(double fadeOutSeconds); // panic then restore

    // check if currently in panic mode
    bool isPanicActive() const { return m_panicActive; }

    // restore from panic to last captured safe state
    void restoreFromPanic(double fadeInSeconds = 1.0);

  signals:
    void lockoutStateChanged(bool locked);
    void panicTriggered();
    void panicCompleted();
    void safeStateCaptured(const QString& description);
    void restorationStarted();
    void restorationCompleted();

  private slots:
    void onFadeProgressChanged(double progress);
    void onPanicFadeComplete();
    void onRestoreFadeComplete();

  private:
    void executePanicFade(double seconds);
    void executeRestoreFade(double seconds);
    void sendSafeValues();

    PlaybackEngine* m_engine;
    MixerProtocol* m_mixer = nullptr;

    // lockout settings
    bool m_lockoutEnabled = true;
    double m_lockoutThreshold = 0.10; // 10%
    bool m_wasLocked = false;

    // safe state
    SafeState m_safeState;
    SafeState m_prePanicState; // state before panic for restoration
    QJsonObject m_defaultSafeValues;

    // panic state
    bool m_panicActive = false;
    bool m_restorationPending = false;
};

} // namespace StageBlend
