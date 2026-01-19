#include "PlaybackEngine.h"
#include "PlaybackGuard.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <cmath>

namespace StageBlend {

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

    // apply fade curve to get curved progress
    double curvedProgress = PlaybackEngine::interpolate(m_progress, m_curve);

    // interpolate all parameters
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
            // for integer values (like mute on/off), switch at midpoint or end
            if (m_progress >= 0.5) {
                m_currentValues[path] = endVal.toInt();
            } else {
                m_currentValues[path] = m_startParams.value(path).toInt();
            }
        } else {
            // for other types, use end value when complete
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

PlaybackEngine::PlaybackEngine(QObject* parent) : QObject(parent) {
    m_fadeTimer.setInterval(16); // ~60fps
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
    // return the maximum progress of all active fades
    double maxProgress = 0.0;
    for (const FadeInstance& fade : m_activeFades) {
        maxProgress = qMax(maxProgress, fade.progress());
    }
    return maxProgress;
}

void PlaybackEngine::go() {
    // if auto-follow is armed (OnButtonPress), execute the pending follow
    if (m_autoFollowArmed) {
        m_autoFollowArmed = false;
        emit autoFollowArmed(false);
    }

    if (!m_cueList || m_standbyIndex < 0)
        return;

    // check lockout
    if (m_guard && m_guard->isGoLocked()) {
        emit goLockout(m_guard->lockoutReason());
        return;
    }

    const Cue& cue = m_cueList->at(m_standbyIndex);

    // validate cue before execution
    if (m_validator) {
        ValidationResult result = m_validator->validate(cue, m_cueList);
        if (!result.valid) {
            emit cueValidationFailed(m_standbyIndex, result);
            return;
        }
    }

    // capture safe state before execution
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
        m_fadeTimer.stop();
        setState(PlaybackState::Paused);
    }
}

void PlaybackEngine::resume() {
    if (m_state == PlaybackState::Paused && !m_activeFades.isEmpty()) {
        // adjust fade start times to account for pause duration
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (FadeInstance& fade : m_activeFades) {
            // this is a simplification - in practice you'd track pause time
            // for now, just restart the timer
        }
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

    // set standby to next cue
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

    switch (cue.type()) {
    case CueType::Snapshot:
        // instant recall
        m_mixer->recallSnapshot(cue);
        setState(PlaybackState::Running);
        handleAutoFollow(cue);
        emit cueCompleted(m_currentIndex);
        break;

    case CueType::Fade:
        startFade(cue);
        break;

    case CueType::Stop:
        // could be used to mute channels or stop something
        m_mixer->recallSnapshot(cue);
        setState(PlaybackState::Running);
        emit cueCompleted(m_currentIndex);
        break;

    case CueType::GoTo:
        // jump to another cue - could parse target from notes or parameters
        break;

    case CueType::Wait:
        // just wait, then auto-follow
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
        // no fade time, just recall instantly
        m_mixer->recallSnapshot(cue);
        emit cueCompleted(m_currentIndex);
        handleAutoFollow(cue);
        return;
    }

    // store start parameters (current mixer state)
    QJsonObject startParams = m_mixer->captureCurrentState();
    QJsonObject endParams = cue.parameters();

    // create new fade instance (allows overlapping fades)
    FadeInstance fade(cue.id(), startParams, endParams, cue.fadeTime(), cue.fadeCurve());
    m_activeFades.append(fade);

    emit fadeStarted(cue.id());
    setState(PlaybackState::Fading);

    // start timer if not already running
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
        // execute all child cues simultaneously
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
        // sequential execution - execute first child, others via auto-follow
        m_currentMacroId = cue.id();
        m_macroChildIndex = 0;

        const Cue* firstChild = m_cueList->findById(childIds.first());
        if (firstChild) {
            executeCueInternal(*firstChild);
            emit macroChildExecuted(cue.id(), childIds.first());
            m_macroChildIndex = 1;

            // continue with next children after this one completes
            // this is handled by checking in onFadeTimerTick / cueCompleted
        }
    }
}

void PlaybackEngine::handleAutoFollow(const Cue& cue) {
    if (!cue.autoFollow()) {
        return;
    }

    switch (cue.autoFollowCondition()) {
    case AutoFollowCondition::Always:
        // start timer immediately with the delay
        m_autoFollowTimer.start(static_cast<int>(cue.autoFollowDelay() * 1000));
        break;

    case AutoFollowCondition::AfterFadeComplete:
        // for fades, timer is started in checkFadeCompletion()
        // for non-fades, start immediately
        if (cue.type() != CueType::Fade) {
            m_autoFollowTimer.start(static_cast<int>(cue.autoFollowDelay() * 1000));
        }
        break;

    case AutoFollowCondition::OnButtonPress:
        // set armed flag, require explicit GO
        m_autoFollowArmed = true;
        emit autoFollowArmed(true);
        break;
    }
}

void PlaybackEngine::checkFadeCompletion() {
    // remove completed fades & emit signals
    for (int i = m_activeFades.size() - 1; i >= 0; --i) {
        if (m_activeFades[i].isComplete()) {
            QString cueId = m_activeFades[i].cueId();
            m_activeFades.removeAt(i);
            emit fadeCompleted(cueId);

            // handle auto-follow for AfterFadeComplete condition
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

    // update state based on remaining fades
    if (m_activeFades.isEmpty()) {
        if (m_state == PlaybackState::Fading) {
            setState(PlaybackState::Running);
            emit cueCompleted(m_currentIndex);
        }
    }
}

void PlaybackEngine::onFadeTimerTick() {
    if (m_activeFades.isEmpty()) {
        m_fadeTimer.stop();
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // merge all active fade values (latest fade wins for conflicts)
    QJsonObject mergedValues;

    for (FadeInstance& fade : m_activeFades) {
        QJsonObject values = fade.update(now);
        for (auto it = values.begin(); it != values.end(); ++it) {
            mergedValues[it.key()] = it.value();
        }
    }

    // send merged values to mixer
    if (m_mixer) {
        for (auto it = mergedValues.begin(); it != mergedValues.end(); ++it) {
            QVariant val = it.value().toVariant();
            m_mixer->sendParameter(it.key(), val);
        }
    }

    // emit overall progress
    emit fadeProgressChanged(fadeProgress());

    // check for completed fades
    checkFadeCompletion();
}

void PlaybackEngine::onAutoFollowTimerTimeout() {
    go(); // execute next cue
}

// static method for fade curve interpolation
double PlaybackEngine::interpolate(double progress, FadeCurve curve) {
    // clamp progress to [0, 1]
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

} // namespace StageBlend
