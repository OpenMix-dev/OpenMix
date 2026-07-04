#include "PlaybackEngine.h"
#include "ActorProfile.h"
#include "ActorProfileLibrary.h"
#include "CueList.h"
#include "DCAMapping.h"
#include "PlaybackGuard.h"
#include "Position.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <QRegularExpression>
#include <cmath>
#include <memory>

namespace OpenMix {

PlaybackEngine::PlaybackEngine(QObject* parent) : QObject(parent) {
    m_autoFollowTimer.setSingleShot(true);
    connect(&m_autoFollowTimer, &QTimer::timeout, this, &PlaybackEngine::onAutoFollowTimerTimeout);
}

void PlaybackEngine::setCueList(CueList* cueList) {
    if (m_cueList)
        m_cueList->disconnect(this);
    m_cueList = cueList;
    stop();
    if (m_cueList) {
        connect(m_cueList, &CueList::cueRemoved, this, &PlaybackEngine::onCueRemoved);
        connect(m_cueList, &CueList::listCleared, this, &PlaybackEngine::onCueListReset);
        connect(m_cueList, &CueList::listLoaded, this, &PlaybackEngine::onCueListReset);
        if (!m_cueList->isEmpty())
            setStandbyIndex(0);
    }
}

void PlaybackEngine::onCueRemoved(int index) {
    // keep our indices valid when a cue is deleted underneath playback
    if (m_currentIndex == index) {
        // the playing cue itself was removed: abandon it and stop follow-on
        m_autoFollowTimer.stop();
        m_autoFollowArmed = false;
        m_currentIndex = -1;
        emit currentCueChanged(m_currentIndex);
    } else if (m_currentIndex > index) {
        --m_currentIndex;
    }

    if (m_standbyIndex == index) {
        int fallback = -1;
        if (m_cueList && !m_cueList->isEmpty())
            fallback = qMin(index, m_cueList->count() - 1);
        m_standbyIndex = -2; // force setStandbyIndex to emit
        setStandbyIndex(fallback);
    } else if (m_standbyIndex > index) {
        setStandbyIndex(m_standbyIndex - 1);
    }
}

void PlaybackEngine::onCueListReset() {
    // the whole list was cleared or replaced (new show / show load): abandon
    // in-flight fades and their stale per-channel sources, then re-arm from the
    // top (stop() leaves standby at 0 when the new list has cues, -1 when empty)
    m_fadeEngine.cancelAll();
    m_appliedChannelLevels.clear();
    stop();
}

void PlaybackEngine::setMixer(MixerProtocol* mixer) { m_mixer = mixer; }

void PlaybackEngine::setDCAMapping(DCAMapping* mapping) { m_dcaMapping = mapping; }

void PlaybackEngine::setActorLibrary(ActorProfileLibrary* library) { m_actorLibrary = library; }

void PlaybackEngine::setPositionLibrary(PositionLibrary* library) { m_positionLibrary = library; }

void PlaybackEngine::setChannelGangs(const QList<QPair<int, int>>& gangs) {
    m_channelGangs = gangs;
    m_gangPartners.clear();
    for (const auto& gang : gangs) {
        if (gang.first == gang.second)
            continue;
        m_gangPartners.insert(gang.first, gang.second);
        m_gangPartners.insert(gang.second, gang.first);
    }
}

void PlaybackEngine::setCheckMode(bool enabled) {
    if (m_checkMode != enabled) {
        m_checkMode = enabled;
        emit checkModeChanged(enabled);
    }
}

void PlaybackEngine::setValidator(CueValidator* validator) { m_validator = validator; }

void PlaybackEngine::setGuard(PlaybackGuard* guard) { m_guard = guard; }

void PlaybackEngine::setLogger(PlaybackLogger* logger) { m_logger = logger; }

const Cue* PlaybackEngine::currentCue() const {
    if (m_cueList && m_currentIndex >= 0 && m_currentIndex < m_cueList->count()) {
        return &m_cueList->at(m_currentIndex);
    }
    return nullptr;
}

const Cue* PlaybackEngine::standbyCue() const {
    if (m_cueList && m_standbyIndex >= 0 && m_standbyIndex < m_cueList->count()) {
        return &m_cueList->at(m_standbyIndex);
    }
    return nullptr;
}

void PlaybackEngine::go() {
    if (m_autoFollowArmed) {
        m_autoFollowArmed = false;
        emit autoFollowArmed(false);
    }

    if (!m_cueList || m_standbyIndex < 0 || m_standbyIndex >= m_cueList->count())
        return;

    if (m_guard && m_guard->isGoLocked()) {
        emit goLockout(m_guard->lockoutReason());
        return;
    }

    const Cue& cue = m_cueList->at(m_standbyIndex);

    if (m_validator) {
        ValidationResult result = m_validator->validate(cue, m_cueList);
        if (!result.valid) {
            emit cueValidationFailed(m_standbyIndex, result);
            return;
        }
    }

    executeCueInternal(cue);

    setCurrentIndex(m_standbyIndex);
    emit cueExecuted(m_currentIndex);

    if (cue.type() == CueType::Snapshot)
        verifyCue(m_currentIndex, cue);

    // in check/soundcheck mode GO holds on the current cue so it can be re-fired
    if (!m_checkMode)
        advanceStandby();
}

void PlaybackEngine::toggleChannelMute(int channel) {
    if (channel <= 0)
        return;
    const bool muted = !m_channelMutes.value(channel, false);
    m_channelMutes[channel] = muted;
    if (m_mixer && m_mixer->isConnected())
        m_mixer->setChannelMute(channel, muted);
    emit channelMuteChanged(channel, muted);
}

void PlaybackEngine::stop() {
    m_autoFollowTimer.stop();
    m_autoFollowArmed = false;

    setState(PlaybackState::Stopped);
    setCurrentIndex(-1);
    if (m_cueList && !m_cueList->isEmpty()) {
        setStandbyIndex(0);
    } else {
        setStandbyIndex(-1);
    }
    emit autoFollowArmed(false);
}

void PlaybackEngine::goToFirst() {
    if (m_cueList && !m_cueList->isEmpty()) {
        setStandbyIndex(0);
    }
}

void PlaybackEngine::goToLast() {
    if (m_cueList && !m_cueList->isEmpty()) {
        setStandbyIndex(m_cueList->count() - 1);
    }
}

void PlaybackEngine::goToIndex(int index) {
    if (m_cueList && index >= 0 && index < m_cueList->count()) {
        setStandbyIndex(index);
    }
}

void PlaybackEngine::goToNumber(double num) {
    if (!m_cueList)
        return;
    auto idx = m_cueList->indexOfNumber(num);
    if (idx.has_value()) {
        setStandbyIndex(idx.value());
    }
}

void PlaybackEngine::previous() {
    if (m_standbyIndex > 0) {
        setStandbyIndex(m_standbyIndex - 1);
    }
}

void PlaybackEngine::next() {
    const int idx = nextEnabledIndex(m_standbyIndex);
    if (idx >= 0) {
        setStandbyIndex(idx);
    }
}

void PlaybackEngine::executeCue(int index) {
    if (!m_cueList || index < 0 || index >= m_cueList->count())
        return;

    const Cue& cue = m_cueList->at(index);
    executeCueInternal(cue);
    setCurrentIndex(index);
    emit cueExecuted(index);

    if (cue.type() == CueType::Snapshot)
        verifyCue(index, cue);

    if (index + 1 < m_cueList->count()) {
        setStandbyIndex(index + 1);
    }
}

void PlaybackEngine::executeCueById(const QString& id) {
    if (!m_cueList)
        return;
    auto index = m_cueList->indexOf(id);
    if (index.has_value()) {
        executeCue(index.value());
    }
}

void PlaybackEngine::setState(PlaybackState state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void PlaybackEngine::setCurrentIndex(int index) {
    if (m_currentIndex != index) {
        m_currentIndex = index;
        emit currentCueChanged(index);
    }
}

void PlaybackEngine::setStandbyIndex(int index) {
    if (m_standbyIndex != index) {
        m_standbyIndex = index;
        emit standbyCueChanged(index);
    }
}

void PlaybackEngine::advanceStandby() {
    if (!m_cueList)
        return;
    // step over skipped cues; nextEnabledIndex returns -1 at end of list
    setStandbyIndex(nextEnabledIndex(m_currentIndex));
}

int PlaybackEngine::nextEnabledIndex(int from) const {
    if (!m_cueList)
        return -1;
    for (int i = from + 1; i < m_cueList->count(); ++i) {
        if (!m_cueList->at(i).skip())
            return i;
    }
    return -1;
}

void PlaybackEngine::executeCueInternal(const Cue& cue) {
    if (!m_mixer)
        return;

    // stop cyclic GoTo/macro chains before they overflow the stack
    if (m_fireDepth >= kMaxFireDepth) {
        qWarning("PlaybackEngine: cue fire recursion limit reached; aborting chain");
        return;
    }
    ++m_fireDepth;
    struct DepthGuard {
        int& depth;
        ~DepthGuard() { --depth; }
    } depthGuard{m_fireDepth};

    // resolve which DCAs to target
    QSet<int> targetDCAs = cue.targetsAllDCAs() ? allDCAs() : cue.targetedDCAs();

    switch (cue.type()) {
    case CueType::Snapshot: {
        // filter params based on DCA targeting (uses cue-specific or show-level mapping)
        QJsonObject filteredParams = filterParametersForDCAs(cue, targetDCAs);

        // instant recall of filtered params
        Cue filteredCue = cue;
        filteredCue.setParameters(filteredParams);
        m_mixer->recallSnapshot(filteredCue);

        // apply each channel's active actor-voice profile on top
        expandChannelProfiles(cue);

        // apply per-channel fader levels (faded over cue.fadeTime if set)
        applyChannelLevels(cue);

        // apply named-position pan/delay assignments
        applyChannelPositions(cue);

        // send per-FX-unit mute states
        applyFxMutes(cue);

        // recall console snippets (partial scene recalls)
        recallSnippets(cue);

        // apply DCA overrides (mute & label only)
        applyDCAOverrides(cue, targetDCAs);

        setState(PlaybackState::Running);
        emit cueCompleted(m_currentIndex);
        handleAutoFollow(cue);
        break;
    }

    case CueType::Stop:
        executeStopCue(cue);
        break;

    case CueType::GoTo:
        executeGoToCue(cue);
        break;

    case CueType::Wait:
        handleAutoFollow(cue);
        emit cueCompleted(m_currentIndex);
        break;

    case CueType::Macro:
        executeMacroCue(cue);
        break;
    }
}

void PlaybackEngine::applyDCAOverrides(const Cue& cue, const QSet<int>& targetDCAs) {
    if (!m_mixer || !m_mixer->isConnected())
        return;

    const MixerCapabilities& caps = m_mixer->capabilities();

    for (int dca : targetDCAs) {
        if (dca < 1 || dca > caps.dcaCount)
            continue;

        DCAOverride override = cue.dcaOverride(dca);

        if (override.mute.has_value()) {
            const bool muting = override.mute.value();
            QString path = QString("/dca/%1/mute").arg(dca);
            m_mixer->sendParameter(path, muting ? 1 : 0);

            // optional console behaviors, only on the muting edge
            if (muting && m_dimDcaFaders)
                m_mixer->sendParameter(QString("/dca/%1/fader").arg(dca), 0.0);
            if (muting && m_muteDcaUnassign)
                m_mixer->sendParameter(QString("/dca/%1/assign").arg(dca), 0);
        }

        if (override.label.has_value()) {
            QString label = override.label.value();
            // truncate to max label length
            if (label.length() > caps.maxDCANameLength) {
                label = label.left(caps.maxDCANameLength);
            }
            QString path = QString("/dca/%1/config/name").arg(dca);
            m_mixer->sendParameter(path, label);
        }
    }
}

void PlaybackEngine::expandChannelProfiles(const Cue& cue) {
    if (!m_mixer)
        return;

    // apply each channel's active actor-profile voice (gain/HPF/EQ/dynamics).
    // the active actor on the channel is resolved live, so an understudy swap
    // changes the applied voice without touching the cue.
    if (m_actorLibrary) {
        const QMap<int, QString> profiles = cue.channelProfiles();
        for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
            std::optional<VoiceData> voice = m_actorLibrary->voiceFor(it.key(), it.value());
            if (voice.has_value())
                applyVoice(it.key(), *voice);
        }
    }
}

void PlaybackEngine::applyChannelLevels(const Cue& cue) {
    if (!m_mixer)
        return;

    const double fadeMs = cue.fadeTime() * 1000.0;
    const FadeCurve curve = cue.fadeCurve();
    const QMap<int, double> levels = cue.channelLevels();

    for (auto it = levels.constBegin(); it != levels.constEnd(); ++it) {
        const int channel = it.key();
        const double target = it.value();
        driveChannelLevel(channel, target, fadeMs, curve);

        // mirror to the ganged partner, unless the cue sets it explicitly
        const auto partner = m_gangPartners.constFind(channel);
        if (partner != m_gangPartners.constEnd() && !levels.contains(partner.value())) {
            driveChannelLevel(partner.value(), target, fadeMs, curve);
        }
    }
}

void PlaybackEngine::driveChannelLevel(int channel, double target, double fadeMs, FadeCurve curve) {
    if (!m_mixer)
        return;

    const double from = m_appliedChannelLevels.value(channel, target);

    if (fadeMs > 0.0 && !qFuzzyCompare(from + 1.0, target + 1.0)) {
        m_fadeEngine.start(QString("ch:%1").arg(channel), from, target, fadeMs, curve,
                           [this, channel](double v) {
                               if (m_mixer)
                                   m_mixer->setChannelFader(channel, v);
                           });
    } else {
        m_mixer->setChannelFader(channel, target);
    }
    m_appliedChannelLevels[channel] = target;
}

void PlaybackEngine::applyChannelPositions(const Cue& cue) {
    if (!m_mixer || !m_positionLibrary)
        return;

    // resolve each channel's assigned Position to pan + delay sends. Paths are the
    // project's OSC-ish convention (matching cue parameters / LoopbackProtocol):
    //   /ch/NN/mix/pan        channel image pan (-1..+1)
    //   /ch/NN/delay/on       per-channel delay enable
    //   /ch/NN/delay/time     per-channel delay (ms)
    //   /ch/NN/mix/BB/pan     pan of the channel's send into bus BB (per-bus imaging)
    const QMap<int, QString> channelPositions = cue.channelPositions();
    for (auto it = channelPositions.constBegin(); it != channelPositions.constEnd(); ++it) {
        const int channel = it.key();
        std::optional<Position> position = m_positionLibrary->position(it.value());
        if (!position.has_value())
            continue;

        const QString ch = QString("%1").arg(channel, 2, 10, QChar('0'));

        m_mixer->sendParameter(QString("/ch/%1/mix/pan").arg(ch), position->pan());

        if (position->delay() > 0.0) {
            m_mixer->sendParameter(QString("/ch/%1/delay/on").arg(ch), 1);
            m_mixer->sendParameter(QString("/ch/%1/delay/time").arg(ch), position->delay());
        }

        for (int bus : position->buses()) {
            const QString busStr = QString("%1").arg(bus, 2, 10, QChar('0'));
            m_mixer->sendParameter(QString("/ch/%1/mix/%2/pan").arg(ch, busStr), position->pan());
        }
    }
}

void PlaybackEngine::applyFxMutes(const Cue& cue) {
    if (!m_mixer)
        return;

    // send each FX unit's mute state to the console (documented path /fx/N/mute)
    const QMap<int, bool> mutes = cue.fxMutes();
    for (auto it = mutes.constBegin(); it != mutes.constEnd(); ++it) {
        m_mixer->sendParameter(QString("/fx/%1/mute").arg(it.key()), it.value() ? 1 : 0);
    }
}

void PlaybackEngine::recallSnippets(const Cue& cue) {
    if (!m_mixer)
        return;

    for (int snippet : cue.snippets()) {
        m_mixer->recallSnippet(snippet);
    }
}

void PlaybackEngine::applyVoice(int channel, const VoiceData& voice) {
    if (voice.gainDb.has_value())
        m_mixer->setChannelPreamp(channel, *voice.gainDb);

    if (voice.hpfOn.has_value() || voice.hpfFreq.has_value())
        m_mixer->setChannelHpf(channel, voice.hpfOn.value_or(true), voice.hpfFreq.value_or(100.0));

    if (voice.eqOn.has_value())
        m_mixer->setChannelEqOn(channel, *voice.eqOn);
    for (const EqBand& b : voice.eqBands)
        m_mixer->setChannelEqBand(channel, b.band, b.on, b.type, b.freq, b.gain, b.q);

    if (voice.dynOn.has_value())
        m_mixer->setChannelDynamics(channel, *voice.dynOn, voice.dynThreshold.value_or(-20.0),
                                    voice.dynRatio.value_or(2.0), voice.dynAttack.value_or(10.0),
                                    voice.dynRelease.value_or(100.0), voice.dynGain.value_or(0.0));
}

void PlaybackEngine::applyBackupSwitch(int coveredChannel, int spareChannel) {
    if (!m_mixer || !m_actorLibrary || spareChannel < 0)
        return;
    const Actor* actor = m_actorLibrary->actorForChannel(coveredChannel);
    if (!actor)
        return;
    // apply the first stored backup voice this actor carries to the spare channel
    for (const QString& slot : actor->profileSlots()) {
        const ActorProfile profile = actor->profile(slot);
        if (!profile.backup().isEmpty()) {
            applyVoice(spareChannel, profile.backup());
            return;
        }
    }
}

void PlaybackEngine::verifyCue(int index, const Cue& cue) {
    if (!m_verifyCues || !m_mixer || !m_mixer->isConnected())
        return;

    // send-only drivers can't read state back; treat as assumed-landed.
    if (!m_mixer->supportsParameterFeedback())
        return;

    // verify the generic snapshot params (sent instantly via recallSnapshot).
    const QSet<int> targetDCAs = cue.targetsAllDCAs() ? allDCAs() : cue.targetedDCAs();
    const QJsonObject params = filterParametersForDCAs(cue, targetDCAs);

    // only numeric params (faders/levels/toggles) are verifiable by value;
    // names/colors are skipped.
    QStringList toVerify;
    for (auto it = params.begin(); it != params.end(); ++it) {
        if (it.value().isDouble())
            toVerify.append(it.key());
    }

    if (toVerify.isEmpty()) {
        emit cueLanded(index);
        return;
    }

    struct Accumulator {
        int remaining;
        int index;
        QStringList drifted;
    };
    auto acc = std::make_shared<Accumulator>(Accumulator{static_cast<int>(toVerify.size()), index, {}});

    for (const QString& path : toVerify) {
        const double expected = params.value(path).toDouble();
        m_mixer->requestParameterAsync(
            path, [this, acc, expected](const QString& p, const QVariant& value, bool success) {
                if (!success || std::abs(value.toDouble() - expected) > 0.01)
                    acc->drifted.append(p);
                if (--acc->remaining == 0) {
                    if (acc->drifted.isEmpty())
                        emit cueLanded(acc->index);
                    else
                        emit cueDrifted(acc->index, acc->drifted);
                }
            });
    }
}

QList<int> PlaybackEngine::getChannelsForDCA(const Cue& cue, int dca) const {
    // check cue-specific mapping first
    if (cue.hasCustomDCAMapping()) {
        return cue.dcaChannelMapping().value(dca);
    }
    // fall back to show-level mapping
    if (m_dcaMapping) {
        return m_dcaMapping->channelsForDCA(dca);
    }
    return {};
}

QList<int> PlaybackEngine::getBusesForDCA(const Cue& cue, int dca) const {
    // check cue-specific mapping first
    if (cue.hasCustomDCAMapping()) {
        return cue.dcaBusMapping().value(dca);
    }
    // fall back to show-level mapping
    if (m_dcaMapping) {
        return m_dcaMapping->busesForDCA(dca);
    }
    return {};
}

QJsonObject PlaybackEngine::filterParametersForDCAs(const Cue& cue,
                                                    const QSet<int>& targetDCAs) const {
    bool hasMapping = cue.hasCustomDCAMapping() || m_dcaMapping;
    if (!hasMapping || targetDCAs.isEmpty()) {
        QJsonObject filtered;
        static QRegularExpression dcaFaderRegex("^/dca/\\d+/fader$");

        // bind to a local: parameters() returns by value and QJsonObject::iterator
        // holds a back-pointer to the object, so iterating the temporary dangles.
        const QJsonObject params = cue.parameters();
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (!dcaFaderRegex.match(it.key()).hasMatch()) {
                filtered[it.key()] = it.value();
            }
        }
        return filtered;
    }

    // set of channels/buses that belong to targeted DCAs
    QSet<int> targetChannels;
    QSet<int> targetBuses;
    for (int dca : targetDCAs) {
        QList<int> channels = getChannelsForDCA(cue, dca);
        for (int ch : channels) {
            targetChannels.insert(ch);
        }
        QList<int> buses = getBusesForDCA(cue, dca);
        for (int bus : buses) {
            targetBuses.insert(bus);
        }
    }

    // filter params to only include targeted channels/buses
    QJsonObject filtered;
    static QRegularExpression channelRegex("^/ch/(\\d+)/");

    static QRegularExpression busRegex("^/bus/(\\d+)/");

    static QRegularExpression dcaFaderRegex("^/dca/\\d+/fader$");

    const QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        const QString& path = it.key();

        if (dcaFaderRegex.match(path).hasMatch()) {
            continue;
        }

        // check if channel parameter
        QRegularExpressionMatch channelMatch = channelRegex.match(path);
        if (channelMatch.hasMatch()) {
            int ch = channelMatch.captured(1).toInt();
            if (targetChannels.contains(ch)) {
                filtered[path] = it.value();
            }
            continue;
        }

        // check if bus parameter
        QRegularExpressionMatch busMatch = busRegex.match(path);
        if (busMatch.hasMatch()) {
            int bus = busMatch.captured(1).toInt();
            if (targetBuses.contains(bus)) {
                filtered[path] = it.value();
            }
            continue;
        }

        // include non-channel/bus parameters (master, etc.)
        filtered[path] = it.value();
    }

    return filtered;
}

QSet<int> PlaybackEngine::allDCAs() const {
    QSet<int> dcas;
    if (m_mixer && m_mixer->isConnected()) {
        int count = m_mixer->capabilities().dcaCount;
        for (int i = 1; i <= count; ++i) {
            dcas.insert(i);
        }
    } else {
        for (int i = 1; i <= m_fallbackDcaCount; ++i) {
            dcas.insert(i);
        }
    }
    return dcas;
}

void PlaybackEngine::executeMacroCue(const Cue& cue) {
    if (!m_cueList)
        return;

    QStringList childIds = cue.childCueIds();
    if (childIds.isEmpty()) {
        emit cueCompleted(m_currentIndex);
        handleAutoFollow(cue);
        return;
    }

    // Parallel and sequential modes both fire the children synchronously in
    // order. Use only local state so a child that is itself a macro cannot
    // clobber this macro's iteration (nested-macro safety). executeCueInternal
    // is recursion-depth-guarded, so a macro cycle cannot overflow the stack.
    const QString macroId = cue.id();
    for (const QString& childId : childIds) {
        const Cue* childCue = m_cueList->findById(childId);
        if (childCue) {
            executeCueInternal(*childCue);
            emit macroChildExecuted(macroId, childId);
        }
    }
    emit cueCompleted(m_currentIndex);
    handleAutoFollow(cue);
}

void PlaybackEngine::executeGoToCue(const Cue& cue) {
    if (!m_cueList)
        return;

    QString target = cue.gotoTarget();
    if (target.isEmpty()) {
        emit cueCompleted(m_currentIndex);
        return;
    }

    int targetIndex = -1;

    bool isNumber = false;
    double cueNumber = target.toDouble(&isNumber);
    if (isNumber) {
        auto result = m_cueList->indexOfNumber(cueNumber);
        if (result.has_value())
            targetIndex = result.value();
    }

    if (targetIndex < 0) {
        auto result = m_cueList->indexOf(target);
        if (result.has_value())
            targetIndex = result.value();
    }

    if (targetIndex < 0) {
        emit cueCompleted(m_currentIndex);
        return;
    }

    setStandbyIndex(targetIndex);

    if (cue.gotoAutoExecute()) {
        go();
    }

    emit cueCompleted(m_currentIndex);
}

void PlaybackEngine::executeStopCue(const Cue& cue) {
    m_autoFollowTimer.stop();
    m_autoFollowArmed = false;

    switch (cue.stopBehavior()) {
    case StopBehavior::StopOnly:
        setState(PlaybackState::Stopped);
        emit cueCompleted(m_currentIndex);
        break;

    case StopBehavior::StopAndApply:
        if (m_mixer && !cue.parameters().isEmpty()) {
            m_mixer->recallSnapshot(cue);
        }
        setState(PlaybackState::Running);
        emit cueCompleted(m_currentIndex);
        handleAutoFollow(cue);
        break;
    }
}

void PlaybackEngine::handleAutoFollow(const Cue& cue) {
    if (!cue.autoFollow()) {
        return;
    }

    switch (cue.autoFollowCondition()) {
    case AutoFollowCondition::Always:
        m_autoFollowTimer.start(static_cast<int>(cue.autoFollowDelay() * 1000));
        break;

    case AutoFollowCondition::OnButtonPress:
        m_autoFollowArmed = true;
        emit autoFollowArmed(true);
        break;
    }
}

void PlaybackEngine::onAutoFollowTimerTimeout() { go(); }

} // namespace OpenMix
