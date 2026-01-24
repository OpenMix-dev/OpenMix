#include "PlaybackEngine.h"
#include "PlaybackGuard.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <QRegularExpression>

namespace OpenMix {

PlaybackEngine::PlaybackEngine(QObject* parent) : QObject(parent) {
    m_autoFollowTimer.setSingleShot(true);
    connect(&m_autoFollowTimer, &QTimer::timeout, this, &PlaybackEngine::onAutoFollowTimerTimeout);
}

void PlaybackEngine::setCueList(CueList* cueList) {
    m_cueList = cueList;
    stop();
    if (m_cueList && !m_cueList->isEmpty()) {
        setStandbyIndex(0);
    }
}

void PlaybackEngine::setMixer(MixerProtocol* mixer) { m_mixer = mixer; }

void PlaybackEngine::setDCAMapping(DCAMapping* mapping) { m_dcaMapping = mapping; }

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

    if (!m_cueList || m_standbyIndex < 0)
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

    advanceStandby();
}

void PlaybackEngine::stop() {
    m_autoFollowTimer.stop();
    m_autoFollowArmed = false;
    m_currentMacroId.clear();
    m_macroChildIndex = 0;
    m_macroPendingChildren.clear();

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
    int idx = m_cueList->indexOfNumber(num);
    if (idx >= 0) {
        setStandbyIndex(idx);
    }
}

void PlaybackEngine::previous() {
    if (m_standbyIndex > 0) {
        setStandbyIndex(m_standbyIndex - 1);
    }
}

void PlaybackEngine::next() {
    if (m_cueList && m_standbyIndex < m_cueList->count() - 1) {
        setStandbyIndex(m_standbyIndex + 1);
    }
}

void PlaybackEngine::executeCue(int index) {
    if (!m_cueList || index < 0 || index >= m_cueList->count())
        return;

    const Cue& cue = m_cueList->at(index);
    executeCueInternal(cue);
    setCurrentIndex(index);
    emit cueExecuted(index);

    if (index + 1 < m_cueList->count()) {
        setStandbyIndex(index + 1);
    }
}

void PlaybackEngine::executeCueById(const QString& id) {
    if (!m_cueList)
        return;
    int index = m_cueList->indexOf(id);
    if (index >= 0) {
        executeCue(index);
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
    if (m_currentIndex + 1 < m_cueList->count()) {
        setStandbyIndex(m_currentIndex + 1);
    } else {
        setStandbyIndex(-1); // end of list
    }
}

void PlaybackEngine::executeCueInternal(const Cue& cue) {
    if (!m_mixer)
        return;

    // resolve which DCAs to target
    QSet<int> targetDCAs = cue.targetsAllDCAs() ? allDCAs() : cue.targetedDCAs();

    switch (cue.type()) {
    case CueType::Snapshot: {
        // filter params based on DCA targeting
        QJsonObject filteredParams = filterParametersForDCAs(cue.parameters(), targetDCAs);

        // instant recall of filtered params
        Cue filteredCue = cue;
        filteredCue.setParameters(filteredParams);
        m_mixer->recallSnapshot(filteredCue);

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
            QString path = QString("/dca/%1/mute").arg(dca);
            m_mixer->sendParameter(path, override.mute.value() ? 1 : 0);
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

QJsonObject PlaybackEngine::filterParametersForDCAs(const QJsonObject& params,
                                                    const QSet<int>& targetDCAs) const {
    if (!m_dcaMapping || targetDCAs.isEmpty()) {
        QJsonObject filtered;
        static QRegularExpression dcaFaderRegex("^/dca/\\d+/fader$");
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
        QList<int> channels = m_dcaMapping->channelsForDCA(dca);
        for (int ch : channels) {
            targetChannels.insert(ch);
        }
        QList<int> buses = m_dcaMapping->busesForDCA(dca);
        for (int bus : buses) {
            targetBuses.insert(bus);
        }
    }

    // filter params to only include targeted channels/buses
    QJsonObject filtered;
    static QRegularExpression channelRegex("^/ch/(\\d+)/");
    static QRegularExpression busRegex("^/bus/(\\d+)/");
    static QRegularExpression dcaFaderRegex("^/dca/\\d+/fader$");

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
        for (int i = 1; i <= 8; ++i) {
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

    if (cue.macroExecutionMode() == MacroExecutionMode::Parallel) {
        for (const QString& childId : childIds) {
            const Cue* childCue = m_cueList->findById(childId);
            if (childCue) {
                executeCueInternal(*childCue);
                emit macroChildExecuted(cue.id(), childId);
            }
        }
        emit cueCompleted(m_currentIndex);
        handleAutoFollow(cue);
    } else {
        m_currentMacroId = cue.id();
        m_macroChildIndex = 0;
        m_macroPendingChildren = childIds;

        const Cue* firstChild = m_cueList->findById(childIds.first());
        if (firstChild) {
            m_macroPendingChildren.removeFirst();
            executeCueInternal(*firstChild);
            emit macroChildExecuted(cue.id(), childIds.first());
            m_macroChildIndex = 1;

            if (m_macroPendingChildren.isEmpty()) {
                const Cue* macroCue = m_cueList->findById(m_currentMacroId);
                m_currentMacroId.clear();
                m_macroPendingChildren.clear();
                m_macroChildIndex = 0;
                emit cueCompleted(m_currentIndex);
                if (macroCue) {
                    handleAutoFollow(*macroCue);
                }
            } else {
                executeNextMacroChild();
            }
        }
    }
}

void PlaybackEngine::executeNextMacroChild() {
    if (m_currentMacroId.isEmpty() || m_macroPendingChildren.isEmpty() || !m_cueList) {
        if (!m_currentMacroId.isEmpty()) {
            const Cue* macroCue = m_cueList->findById(m_currentMacroId);

            m_currentMacroId.clear();
            m_macroPendingChildren.clear();
            m_macroChildIndex = 0;

            emit cueCompleted(m_currentIndex);

            if (macroCue) {
                handleAutoFollow(*macroCue);
            }
        }
        return;
    }

    QString childId = m_macroPendingChildren.takeFirst();
    const Cue* childCue = m_cueList->findById(childId);
    if (childCue) {
        executeCueInternal(*childCue);
        emit macroChildExecuted(m_currentMacroId, childId);
        m_macroChildIndex++;

        if (!m_macroPendingChildren.isEmpty()) {
            executeNextMacroChild();
        } else {
            const Cue* macroCue = m_cueList->findById(m_currentMacroId);
            m_currentMacroId.clear();
            m_macroPendingChildren.clear();
            m_macroChildIndex = 0;
            emit cueCompleted(m_currentIndex);
            if (macroCue) {
                handleAutoFollow(*macroCue);
            }
        }
    } else {
        executeNextMacroChild();
    }
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
        targetIndex = m_cueList->indexOfNumber(cueNumber);
    }

    if (targetIndex < 0) {
        targetIndex = m_cueList->indexOf(target);
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

    m_currentMacroId.clear();
    m_macroPendingChildren.clear();
    m_macroChildIndex = 0;

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
