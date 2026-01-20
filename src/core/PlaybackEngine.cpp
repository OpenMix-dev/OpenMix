#include "PlaybackEngine.h"
#include "PlaybackGuard.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <cmath>

namespace OpenMix {

FadeInstance::FadeInstance(const QString& cueId, const QJsonObject& startParams,
                           const QJsonObject& endParams, double durationSec, FadeCurve curve)
    : m_cueId(cueId), m_startParams(startParams), m_endParams(endParams), m_duration(durationSec),
      m_startTime(QDateTime::currentMSecsSinceEpoch()), m_progress(0.0), m_curve(curve) {}

QJsonObject FadeInstance::update(qint64 currentTime) {
    if (m_duration <= 0) {
        m_progress = 1.0;
        m_currentValues = m_endParams;
        return m_currentValues;
    }

    qint64 elapsed = currentTime - m_startTime;
    m_progress = qMin(1.0, elapsed / (m_duration * 1000.0));

    double curvedProgress = PlaybackEngine::interpolate(m_progress, m_curve);

    m_currentValues = QJsonObject();
    for (auto it = m_endParams.begin(); it != m_endParams.end(); ++it) {
        QString path = it.key();
        QVariant endVal = it.value().toVariant();

        if (endVal.typeId() == QMetaType::Double || endVal.typeId() == QMetaType::Float) {
            double startVal = m_startParams.value(path).toDouble();
            double targetVal = endVal.toDouble();
            double currentVal = startVal + (targetVal - startVal) * curvedProgress;
            m_currentValues[path] = currentVal;
        } else if (endVal.typeId() == QMetaType::Int) {
            if (m_progress >= 0.5) {
                m_currentValues[path] = endVal.toInt();
            } else {
                m_currentValues[path] = m_startParams.value(path).toInt();
            }
        } else {
            if (m_progress >= 1.0) {
                m_currentValues[path] = it.value();
            }
        }
    }

    return m_currentValues;
}

QVariant FadeInstance::interpolatedValue(const QString& path) const {
    if (m_currentValues.contains(path)) {
        return m_currentValues[path].toVariant();
    }
    return QVariant();
}

void FadeInstance::adjustStartTime(qint64 offset) { m_startTime += offset; }

PlaybackEngine::PlaybackEngine(QObject* parent) : QObject(parent) {
    m_fadeTimer.setInterval(16);
    connect(&m_fadeTimer, &QTimer::timeout, this, &PlaybackEngine::onFadeTimerTick);

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

void PlaybackEngine::setValidator(CueValidator* validator) { m_validator = validator; }

void PlaybackEngine::setGuard(PlaybackGuard* guard) { m_guard = guard; }

void PlaybackEngine::setLogger(PlaybackLogger* logger) { m_logger = logger; }

void PlaybackEngine::setConflictResolver(FadeConflictResolver* resolver) {
    m_conflictResolver = resolver;
}

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

double PlaybackEngine::fadeProgress() const {
    if (m_activeFades.isEmpty()) {
        return 0.0;
    }
    double maxProgress = 0.0;
    for (const FadeInstance& fade : m_activeFades) {
        maxProgress = qMax(maxProgress, fade.progress());
    }
    return maxProgress;
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

    if (m_guard) {
        QString cueNumber = QString::number(cue.number(), 'f', 1);
        m_guard->captureSafeState(tr("Before cue %1").arg(cueNumber));
    }

    executeCueInternal(cue);

    setCurrentIndex(m_standbyIndex);
    emit cueExecuted(m_currentIndex);

    advanceStandby();
}

void PlaybackEngine::stop() {
    m_fadeTimer.stop();
    m_autoFollowTimer.stop();
    m_activeFades.clear();
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
    emit fadeProgressChanged(0.0);
    emit autoFollowArmed(false);
}

void PlaybackEngine::pause() {
    if (m_state == PlaybackState::Fading) {
        m_pauseStartTime = QDateTime::currentMSecsSinceEpoch();
        m_fadeTimer.stop();
        setState(PlaybackState::Paused);
    }
}

void PlaybackEngine::resume() {
    if (m_state == PlaybackState::Paused && !m_activeFades.isEmpty()) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 pauseDuration = now - m_pauseStartTime;

        for (FadeInstance& fade : m_activeFades) {
            fade.adjustStartTime(pauseDuration);
        }

        m_pauseStartTime = 0;
        m_fadeTimer.start();
        setState(PlaybackState::Fading);
    }
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

void PlaybackEngine::executePanicFade(const QJsonObject& targetValues, double durationSec,
                                      FadeCurve curve) {
    if (!m_mixer || targetValues.isEmpty())
        return;

    cancelPanicFade();

    QJsonObject startParams = m_mixer->captureCurrentState();

    m_panicFadeId = "panic_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    m_panicFadeActive = true;

    FadeInstance fade(m_panicFadeId, startParams, targetValues, durationSec, curve);
    m_activeFades.append(fade);

    setState(PlaybackState::Fading);

    if (!m_fadeTimer.isActive()) {
        m_fadeTimer.start();
    }
}

void PlaybackEngine::cancelPanicFade() {
    if (!m_panicFadeActive)
        return;

    for (int i = m_activeFades.size() - 1; i >= 0; --i) {
        if (m_activeFades[i].cueId() == m_panicFadeId) {
            m_activeFades.removeAt(i);
            break;
        }
    }

    m_panicFadeActive = false;
    m_panicFadeId.clear();

    if (m_activeFades.isEmpty() && m_state == PlaybackState::Fading) {
        setState(PlaybackState::Running);
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

    switch (cue.type()) {
    case CueType::Snapshot:
        m_mixer->recallSnapshot(cue);
        setState(PlaybackState::Running);
        handleAutoFollow(cue);
        emit cueCompleted(m_currentIndex);
        break;

    case CueType::Fade:
        startFade(cue);
        break;

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

void PlaybackEngine::startFade(const Cue& cue) {
    if (cue.fadeTime() <= 0) {
        m_mixer->recallSnapshot(cue);
        emit cueCompleted(m_currentIndex);
        handleAutoFollow(cue);
        return;
    }

    QJsonObject startParams = m_mixer->captureCurrentState();
    QJsonObject endParams = cue.parameters();

    FadeInstance fade(cue.id(), startParams, endParams, cue.fadeTime(), cue.fadeCurve());
    m_activeFades.append(fade);

    emit fadeStarted(cue.id());
    setState(PlaybackState::Fading);

    if (!m_fadeTimer.isActive()) {
        m_fadeTimer.start();
    }
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
    m_fadeTimer.stop();
    m_activeFades.clear();
    m_autoFollowTimer.stop();
    m_autoFollowArmed = false;

    m_currentMacroId.clear();
    m_macroPendingChildren.clear();
    m_macroChildIndex = 0;

    switch (cue.stopBehavior()) {
    case StopBehavior::StopFadesOnly:
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

    case StopBehavior::StopAndFadeOut:
        if (m_mixer && !cue.parameters().isEmpty() && cue.fadeTime() > 0) {
            startFade(cue);
        } else {
            if (m_mixer && !cue.parameters().isEmpty()) {
                m_mixer->recallSnapshot(cue);
            }
            setState(PlaybackState::Running);
            emit cueCompleted(m_currentIndex);
            handleAutoFollow(cue);
        }
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

    case AutoFollowCondition::AfterFadeComplete:
        if (cue.type() != CueType::Fade) {
            m_autoFollowTimer.start(static_cast<int>(cue.autoFollowDelay() * 1000));
        }
        break;

    case AutoFollowCondition::OnButtonPress:
        m_autoFollowArmed = true;
        emit autoFollowArmed(true);
        break;
    }
}

void PlaybackEngine::checkFadeCompletion() {
    bool panicFadeWasCompleted = false;

    for (int i = m_activeFades.size() - 1; i >= 0; --i) {
        if (m_activeFades[i].isComplete()) {
            QString cueId = m_activeFades[i].cueId();
            m_activeFades.removeAt(i);

            if (cueId == m_panicFadeId) {
                m_panicFadeActive = false;
                m_panicFadeId.clear();
                panicFadeWasCompleted = true;
                continue;
            }

            emit fadeCompleted(cueId);

            if (m_cueList) {
                const Cue* completedCue = m_cueList->findById(cueId);
                if (completedCue && completedCue->autoFollow() &&
                    completedCue->autoFollowCondition() == AutoFollowCondition::AfterFadeComplete) {
                    m_autoFollowTimer.start(
                        static_cast<int>(completedCue->autoFollowDelay() * 1000));
                }
            }
        }
    }

    if (panicFadeWasCompleted) {
        emit panicFadeCompleted();
    }

    if (m_activeFades.isEmpty()) {
        if (m_state == PlaybackState::Fading) {
            setState(PlaybackState::Running);

            if (!m_currentMacroId.isEmpty()) {
                executeNextMacroChild();
            } else if (!panicFadeWasCompleted) {
                emit cueCompleted(m_currentIndex);
            }
        }
    }
}

void PlaybackEngine::onFadeTimerTick() {
    if (m_activeFades.isEmpty()) {
        m_fadeTimer.stop();
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    QJsonObject mergedValues;

    for (FadeInstance& fade : m_activeFades) {
        QJsonObject values = fade.update(now);
        for (auto it = values.begin(); it != values.end(); ++it) {
            mergedValues[it.key()] = it.value();
        }
    }

    if (m_mixer) {
        for (auto it = mergedValues.begin(); it != mergedValues.end(); ++it) {
            QVariant val = it.value().toVariant();
            m_mixer->sendParameter(it.key(), val);
        }
    }

    emit fadeProgressChanged(fadeProgress());

    checkFadeCompletion();
}

void PlaybackEngine::onAutoFollowTimerTimeout() { go(); }

double PlaybackEngine::interpolate(double progress, FadeCurve curve) {
    progress = qBound(0.0, progress, 1.0);

    switch (curve) {
    case FadeCurve::Linear:
        return progress;

    case FadeCurve::EaseIn:
        // quadratic ease in: progress^2
        return progress * progress;

    case FadeCurve::EaseOut:
        // quadratic ease out: 1 - (1 - progress)^2
        return 1.0 - (1.0 - progress) * (1.0 - progress);

    case FadeCurve::SCurve:
        // smooth S-curve (ease in-out)
        if (progress < 0.5) {
            return 2.0 * progress * progress;
        } else {
            return 1.0 - std::pow(-2.0 * progress + 2.0, 2.0) / 2.0;
        }

    case FadeCurve::Exponential:
        // exponential curve for audio-like response
        // (exp(progress * 3) - 1) / (exp(3) - 1)
        return (std::exp(progress * 3.0) - 1.0) / (std::exp(3.0) - 1.0);

    default:
        return progress;
    }
}

} // namespace OpenMix
