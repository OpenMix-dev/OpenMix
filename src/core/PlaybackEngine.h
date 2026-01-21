#pragma once

#include "Cue.h"
#include "CueList.h"
#include "CueValidator.h"
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace OpenMix {

class MixerProtocol;
class PlaybackGuard;
class PlaybackLogger;
class FadeConflictResolver;
class LiveEditSession;

enum class PlaybackState { Stopped, Running, Fading, Paused };

class FadeInstance {
  public:
    FadeInstance() = default;
    FadeInstance(const QString& cueId, const QJsonObject& startParams, const QJsonObject& endParams,
                 double durationSec, FadeCurve curve);

    QString cueId() const { return m_cueId; }
    bool isComplete() const { return m_progress >= 1.0; }
    double progress() const { return m_progress; }
    FadeCurve curve() const { return m_curve; }
    qint64 startTime() const { return m_startTime; }
    double duration() const { return m_duration; }

    QJsonObject update(qint64 currentTime);
    QVariant interpolatedValue(const QString& path) const;
    void adjustStartTime(qint64 offset);

  private:
    QString m_cueId;
    QJsonObject m_startParams;
    QJsonObject m_endParams;
    double m_duration = 0.0;
    qint64 m_startTime = 0;
    double m_progress = 0.0;
    FadeCurve m_curve = FadeCurve::Linear;
    QJsonObject m_currentValues;
};

class PlaybackEngine : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackEngine(QObject* parent = nullptr);

    void setCueList(CueList* cueList);
    void setMixer(MixerProtocol* mixer);

    void setValidator(CueValidator* validator);
    CueValidator* validator() const { return m_validator; }

    void setGuard(PlaybackGuard* guard);
    PlaybackGuard* guard() const { return m_guard; }

    void setLogger(PlaybackLogger* logger);
    PlaybackLogger* logger() const { return m_logger; }

    void setConflictResolver(FadeConflictResolver* resolver);
    FadeConflictResolver* conflictResolver() const { return m_conflictResolver; }

    void setLiveEditSession(LiveEditSession* session);
    LiveEditSession* liveEditSession() const { return m_liveEditSession; }

    void setDryRunMode(bool enabled) { m_dryRunMode = enabled; }
    bool isDryRunMode() const { return m_dryRunMode; }

    PlaybackState state() const { return m_state; }

    int currentCueIndex() const { return m_currentIndex; }
    int standbyCueIndex() const { return m_standbyIndex; }

    const Cue* currentCue() const;
    const Cue* standbyCue() const;

    double fadeProgress() const;
    int activeFadeCount() const { return m_activeFades.size(); }
    bool isAutoFollowArmed() const { return m_autoFollowArmed; }

    static double interpolate(double progress, FadeCurve curve);

  public slots:
    void go();
    void stop();
    void pause();
    void resume();

    void goToFirst();
    void goToLast();
    void goToIndex(int index);
    void goToNumber(double num);
    void previous();
    void next();

    void executeCue(int index);
    void executeCueById(const QString& id);

    void executePanicFade(const QJsonObject& targetValues, double durationSec, FadeCurve curve);
    void cancelPanicFade();

  signals:
    void stateChanged(PlaybackState state);
    void currentCueChanged(int index);
    void standbyCueChanged(int index);
    void fadeProgressChanged(double progress);
    void fadeStarted(const QString& cueId);
    void fadeCompleted(const QString& cueId);
    void cueExecuted(int index);
    void cueCompleted(int index);
    void autoFollowArmed(bool armed);
    void macroChildExecuted(const QString& parentId, const QString& childId);

    void cueValidationFailed(int index, const ValidationResult& result);
    void goLockout(const QString& reason);
    void emergencyStopped();
    void panicFadeCompleted();

  private slots:
    void onFadeTimerTick();
    void onAutoFollowTimerTimeout();

  private:
    void setState(PlaybackState state);
    void setCurrentIndex(int index);
    void setStandbyIndex(int index);
    void advanceStandby();
    void executeCueInternal(const Cue& cue);
    void startFade(const Cue& cue);
    void executeMacroCue(const Cue& cue);
    void executeGoToCue(const Cue& cue);
    void executeStopCue(const Cue& cue);
    void executeNextMacroChild();
    void handleAutoFollow(const Cue& cue);
    void checkFadeCompletion();

    CueList* m_cueList = nullptr;
    MixerProtocol* m_mixer = nullptr;
    PlaybackState m_state = PlaybackState::Stopped;
    int m_currentIndex = -1;
    int m_standbyIndex = -1;

    QTimer m_fadeTimer;
    QVector<FadeInstance> m_activeFades;

    QTimer m_autoFollowTimer;
    bool m_autoFollowArmed = false;

    QString m_currentMacroId;
    int m_macroChildIndex = 0;
    QStringList m_macroPendingChildren;

    CueValidator* m_validator = nullptr;
    PlaybackGuard* m_guard = nullptr;
    PlaybackLogger* m_logger = nullptr;
    FadeConflictResolver* m_conflictResolver = nullptr;
    LiveEditSession* m_liveEditSession = nullptr;

    bool m_dryRunMode = false;
    qint64 m_pauseStartTime = 0;
    bool m_panicFadeActive = false;
    QString m_panicFadeId;
};

} // namespace OpenMix
