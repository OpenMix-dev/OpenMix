#pragma once

#include "Cue.h"
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace OpenMix {

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

    void setMixer(MixerProtocol* mixer);

    void setLockoutEnabled(bool enabled);
    bool isLockoutEnabled() const { return m_lockoutEnabled; }

    void setLockoutThreshold(double progressPercent);
    double lockoutThreshold() const { return m_lockoutThreshold; }

    bool isGoLocked() const;
    QString lockoutReason() const;

    void captureSafeState(const QString& description);
    void clearSafeState();
    bool hasSafeState() const { return !m_safeState.parameters.isEmpty(); }
    SafeState safeState() const { return m_safeState; }

    void setDefaultSafeValues(const QJsonObject& values);
    QJsonObject defaultSafeValues() const { return m_defaultSafeValues; }

    void setPanicFadeCurve(FadeCurve curve) { m_panicFadeCurve = curve; }
    FadeCurve panicFadeCurve() const { return m_panicFadeCurve; }

    void panic(double fadeOutSeconds = 0.5);
    void panicImmediate();
    void panicAndRestore(double fadeOutSeconds);

    bool isPanicActive() const { return m_panicActive; }

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

    bool m_lockoutEnabled = true;
    double m_lockoutThreshold = 0.10;
    bool m_wasLocked = false;

    SafeState m_safeState;
    SafeState m_prePanicState;
    QJsonObject m_defaultSafeValues;

    bool m_panicActive = false;
    bool m_restorationPending = false;
    bool m_restoreFadeActive = false;

    FadeCurve m_panicFadeCurve = FadeCurve::Linear;
};

} // namespace OpenMix
