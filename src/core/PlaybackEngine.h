#pragma once

#include "Cue.h"
#include "CueList.h"
#include "CueValidator.h"
#include "DCAMapping.h"
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace OpenMix {

class MixerProtocol;
class PlaybackGuard;
class PlaybackLogger;

enum class PlaybackState { Stopped, Running };

class PlaybackEngine : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackEngine(QObject* parent = nullptr);

    void setCueList(CueList* cueList);
    void setMixer(MixerProtocol* mixer);
    void setDCAMapping(DCAMapping* mapping);

    void setValidator(CueValidator* validator);
    CueValidator* validator() const { return m_validator; }

    void setGuard(PlaybackGuard* guard);
    PlaybackGuard* guard() const { return m_guard; }

    void setLogger(PlaybackLogger* logger);
    PlaybackLogger* logger() const { return m_logger; }

    void setDryRunMode(bool enabled) { m_dryRunMode = enabled; }
    bool isDryRunMode() const { return m_dryRunMode; }

    PlaybackState state() const { return m_state; }

    int currentCueIndex() const { return m_currentIndex; }
    int standbyCueIndex() const { return m_standbyIndex; }

    const Cue* currentCue() const;
    const Cue* standbyCue() const;

    bool isAutoFollowArmed() const { return m_autoFollowArmed; }

  public slots:
    void go();
    void stop();

    void goToFirst();
    void goToLast();
    void goToIndex(int index);
    void goToNumber(double num);
    void previous();
    void next();
    void setStandbyIndex(int index);

    void executeCue(int index);
    void executeCueById(const QString& id);

  signals:
    void stateChanged(PlaybackState state);
    void currentCueChanged(int index);
    void standbyCueChanged(int index);
    void cueExecuted(int index);
    void cueCompleted(int index);
    void autoFollowArmed(bool armed);
    void macroChildExecuted(const QString& parentId, const QString& childId);

    void cueValidationFailed(int index, const ValidationResult& result);
    void goLockout(const QString& reason);
    void emergencyStopped();

  private slots:
    void onAutoFollowTimerTimeout();

  private:
    void setState(PlaybackState state);
    void setCurrentIndex(int index);
    void advanceStandby();
    void executeCueInternal(const Cue& cue);
    void applyDCAOverrides(const Cue& cue, const QSet<int>& targetDCAs);
    void executeMacroCue(const Cue& cue);
    void executeGoToCue(const Cue& cue);
    void executeStopCue(const Cue& cue);
    void executeNextMacroChild();
    void handleAutoFollow(const Cue& cue);

    // DCA filtering helpers
    QList<int> getChannelsForDCA(const Cue& cue, int dca) const;
    QList<int> getBusesForDCA(const Cue& cue, int dca) const;
    QJsonObject filterParametersForDCAs(const Cue& cue, const QSet<int>& targetDCAs) const;
    QSet<int> allDCAs() const;

    CueList* m_cueList = nullptr;
    MixerProtocol* m_mixer = nullptr;
    DCAMapping* m_dcaMapping = nullptr;
    PlaybackState m_state = PlaybackState::Stopped;
    int m_currentIndex = -1;
    int m_standbyIndex = -1;

    QTimer m_autoFollowTimer;
    bool m_autoFollowArmed = false;

    QString m_currentMacroId;
    int m_macroChildIndex = 0;
    QStringList m_macroPendingChildren;

    CueValidator* m_validator = nullptr;
    PlaybackGuard* m_guard = nullptr;
    PlaybackLogger* m_logger = nullptr;

    bool m_dryRunMode = false;
};

} // namespace OpenMix
