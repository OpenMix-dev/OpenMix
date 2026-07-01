#pragma once

#include "CueValidator.h"
#include "FadeEngine.h"
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QTimer>

namespace OpenMix {

class Cue;
class CueList;
class DCAMapping;
class MixerProtocol;
class PlaybackGuard;
class PlaybackLogger;
class ActorProfileLibrary;
class PositionLibrary;
struct VoiceData;

enum class PlaybackState { Stopped, Running };

class PlaybackEngine : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackEngine(QObject* parent = nullptr);

    void setCueList(CueList* cueList);
    void setMixer(MixerProtocol* mixer);
    void setDCAMapping(DCAMapping* mapping);
    void setActorLibrary(ActorProfileLibrary* library);
    void setPositionLibrary(PositionLibrary* library);

    // ganged input-channel pairs (from the show); a level applied to one channel
    // is mirrored to its partner on fire.
    void setChannelGangs(const QList<QPair<int, int>>& gangs);

    // apply the backup voice of the actor covering `coveredChannel` to the spare
    // channel (spare-backup mic switch). No-op without a mixer/actor/backup voice.
    void applyBackupSwitch(int coveredChannel, int spareChannel);
    [[nodiscard]] QList<QPair<int, int>> channelGangs() const { return m_channelGangs; }

    // check / soundcheck (rehearsal) mode: while enabled, GO re-fires the current
    // cue instead of advancing standby to the next cue.
    void setCheckMode(bool enabled);
    [[nodiscard]] bool checkMode() const noexcept { return m_checkMode; }

    // drives timed fader fades; exposed so the UI (and tests) can observe/step it
    [[nodiscard]] FadeEngine* fadeEngine() { return &m_fadeEngine; }

    // when enabled, after a cue applies, read changed params back from the console
    // and emit cueLanded / cueDrifted (no-op on send-only drivers)
    void setVerifyCues(bool enabled) { m_verifyCues = enabled; }
    [[nodiscard]] bool verifyCues() const noexcept { return m_verifyCues; }

    // console-behavior toggles applied during DCA override recall (mirrors show
    // preferences; both default off so existing recalls are unchanged):
    //   dimDcaFaders    - pull a DCA's fader to unity-off when a cue mutes it
    //   muteDcaUnassign - drop a DCA's channel assignments when a cue mutes it
    void setDimDcaFaders(bool enabled) { m_dimDcaFaders = enabled; }
    [[nodiscard]] bool dimDcaFaders() const noexcept { return m_dimDcaFaders; }
    void setMuteDcaUnassign(bool enabled) { m_muteDcaUnassign = enabled; }
    [[nodiscard]] bool muteDcaUnassign() const noexcept { return m_muteDcaUnassign; }

    // channel mute state driven by MIDI mute buttons (console-style mute toggle)
    [[nodiscard]] bool isChannelMuted(int channel) const { return m_channelMutes.value(channel); }

    void setValidator(CueValidator* validator);
    [[nodiscard]] CueValidator* validator() const noexcept { return m_validator; }

    void setGuard(PlaybackGuard* guard);
    [[nodiscard]] PlaybackGuard* guard() const noexcept { return m_guard; }

    void setLogger(PlaybackLogger* logger);
    [[nodiscard]] PlaybackLogger* logger() const noexcept { return m_logger; }

    void setDryRunMode(bool enabled) { m_dryRunMode = enabled; }
    [[nodiscard]] bool isDryRunMode() const noexcept { return m_dryRunMode; }

    [[nodiscard]] PlaybackState state() const noexcept { return m_state; }

    [[nodiscard]] int currentCueIndex() const noexcept { return m_currentIndex; }
    [[nodiscard]] int standbyCueIndex() const noexcept { return m_standbyIndex; }

    [[nodiscard]] const Cue* currentCue() const;
    [[nodiscard]] const Cue* standbyCue() const;

    [[nodiscard]] bool isAutoFollowArmed() const noexcept { return m_autoFollowArmed; }

  public slots:
    void go();
    void stop();

    // toggle a channel's mute on the console and remember the new state
    void toggleChannelMute(int channel);

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
    void cueLanded(int index);
    void cueDrifted(int index, const QStringList& paths);
    void autoFollowArmed(bool armed);
    void macroChildExecuted(const QString& parentId, const QString& childId);
    void checkModeChanged(bool enabled);

    void cueValidationFailed(int index, const ValidationResult& result);
    void goLockout(const QString& reason);
    void emergencyStopped();
    void channelMuteChanged(int channel, bool muted);

  private slots:
    void onAutoFollowTimerTimeout();

  private:
    void setState(PlaybackState state);
    void setCurrentIndex(int index);
    void advanceStandby();
    // first index after 'from' whose cue is not skipped, or -1 at end of list
    [[nodiscard]] int nextEnabledIndex(int from) const;
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

    // actor voice profiles: apply each channel's active profile + level on fire
    void expandChannelProfiles(const Cue& cue);
    void applyVoice(int channel, const VoiceData& voice);
    void applyChannelLevels(const Cue& cue); // fade-aware per-channel fader moves
    // drive a single channel's fader to target (instant or faded), tracking the
    // applied value as the next fade's source.
    void driveChannelLevel(int channel, double target, double fadeMs, FadeCurve curve);
    void applyChannelPositions(const Cue& cue); // named-position pan/delay sends
    void applyFxMutes(const Cue& cue);          // per-FX-unit mute sends
    void recallSnippets(const Cue& cue);        // console snippet recalls
    void verifyCue(int index, const Cue& cue);

    CueList* m_cueList = nullptr;
    MixerProtocol* m_mixer = nullptr;
    DCAMapping* m_dcaMapping = nullptr;
    ActorProfileLibrary* m_actorLibrary = nullptr;
    PositionLibrary* m_positionLibrary = nullptr;

    FadeEngine m_fadeEngine;
    QMap<int, double> m_appliedChannelLevels; // last-applied fader per channel (fade source)
    QList<QPair<int, int>> m_channelGangs;    // ganged input-channel pairs
    QHash<int, int> m_gangPartners;           // channel -> ganged partner (both directions)
    bool m_checkMode = false;                 // soundcheck: GO holds on current cue
    bool m_verifyCues = false;
    bool m_dimDcaFaders = false;              // dim a DCA's fader when a cue mutes it
    bool m_muteDcaUnassign = false;           // unassign a DCA's channels when muted
    QHash<int, bool> m_channelMutes;          // channel -> mute state (MIDI mute buttons)
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
