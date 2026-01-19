#pragma once

#include "Cue.h"
#include "CueList.h"
#include "CueValidator.h"
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace StageBlend {

class MixerProtocol;
class PlaybackGuard;
class PlaybackLogger;
class FadeConflictResolver;

enum class PlaybackState { Stopped, Running, Fading, Paused };

// individual fade instance for overlapping fades
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

    // update progress & return interpolated values for this fade
    QJsonObject update(qint64 currentTime);

    // get current interpolated value for a specific parameter
    QVariant interpolatedValue(const QString& path) const;

  private:
    QString m_cueId;
    QJsonObject m_startParams;
    QJsonObject m_endParams;
    double m_duration = 0.0; // seconds
    qint64 m_startTime = 0;
    double m_progress = 0.0;
    FadeCurve m_curve = FadeCurve::Linear;
    QJsonObject m_currentValues; // last computed interpolated values
};

class PlaybackEngine : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackEngine(QObject* parent = nullptr);

    // setup
    void setCueList(CueList* cueList);
    void setMixer(MixerProtocol* mixer);

    // safety components
    void setValidator(CueValidator* validator);
    CueValidator* validator() const { return m_validator; }

    void setGuard(PlaybackGuard* guard);
    PlaybackGuard* guard() const { return m_guard; }

    void setLogger(PlaybackLogger* logger);
    PlaybackLogger* logger() const { return m_logger; }

    void setConflictResolver(FadeConflictResolver* resolver);
    FadeConflictResolver* conflictResolver() const { return m_conflictResolver; }

    // dry run mode (no mixer sends)
    void setDryRunMode(bool enabled) { m_dryRunMode = enabled; }
    bool isDryRunMode() const { return m_dryRunMode; }

    // playback state
    PlaybackState state() const { return m_state; }

    // current/standby cue indices (-1 if none)
    int currentCueIndex() const { return m_currentIndex; }
    int standbyCueIndex() const { return m_standbyIndex; }

    // get cue pointers
    const Cue* currentCue() const;
    const Cue* standbyCue() const;

    // fade progress (0.0 - 1.0) - returns max progress of all active fades
    double fadeProgress() const;

    // active fade count
    int activeFadeCount() const { return m_activeFades.size(); }

    // check if auto-follow is armed (waiting for button press)
    bool isAutoFollowArmed() const { return m_autoFollowArmed; }

    // fade curve interpolation (used by FadeInstance)
    static double interpolate(double progress, FadeCurve curve);

  public slots:
    // main playback control
    void go();     // execute standby cue
    void stop();   // stop playback
    void pause();  // pause all fades
    void resume(); // resume all fades

    // navigation
    void goToFirst();            // set standby to first cue
    void goToLast();             // set standby to last cue
    void goToIndex(int index);   // set standby to specific index
    void goToNumber(double num); // set standby to cue number
    void previous();             // move standby to previous cue
    void next();                 // move standby to next cue

    // direct execution
    void executeCue(int index);             // execute specific cue immediately
    void executeCueById(const QString& id); // execute by cue ID

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

    // safety signals
    void cueValidationFailed(int index, const ValidationResult& result);
    void goLockout(const QString& reason);
    void emergencyStopped();

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
    void handleAutoFollow(const Cue& cue);
    void checkFadeCompletion();

    CueList* m_cueList = nullptr;
    MixerProtocol* m_mixer = nullptr;
    PlaybackState m_state = PlaybackState::Stopped;
    int m_currentIndex = -1;
    int m_standbyIndex = -1;

    // multiple active fades for overlapping support
    QTimer m_fadeTimer;
    QVector<FadeInstance> m_activeFades;

    // auto-follow
    QTimer m_autoFollowTimer;
    bool m_autoFollowArmed = false; // for OnButtonPress condition

    // track current macro execution
    QString m_currentMacroId;
    int m_macroChildIndex = 0;

    // safety components
    CueValidator* m_validator = nullptr;
    PlaybackGuard* m_guard = nullptr;
    PlaybackLogger* m_logger = nullptr;
    FadeConflictResolver* m_conflictResolver = nullptr;

    // dry run mode
    bool m_dryRunMode = false;
};

} // namespace StageBlend
