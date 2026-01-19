#include "DryRunEngine.h"
#include "Cue.h"
#include "CueList.h"
#include "CueValidator.h"
#include <QSet>
#include <cmath>

namespace StageBlend {

DryRunEngine::DryRunEngine(QObject* parent) : QObject(parent) {}

void DryRunEngine::setCueList(CueList* cueList) { m_cueList = cueList; }

void DryRunEngine::setValidator(CueValidator* validator) { m_validator = validator; }

void DryRunEngine::setInitialState(const QJsonObject& state) { m_initialState = state; }

DryRunResult DryRunEngine::executeDryRun(int cueIndex) {
    DryRunResult result;
    result.wouldSucceed = false;
    result.validationPassed = true;
    result.totalDuration = 0.0;

    if (!m_cueList || cueIndex < 0 || cueIndex >= m_cueList->count()) {
        result.warnings.append("Invalid cue index");
        return result;
    }

    emit dryRunStarted(cueIndex);

    const Cue& cue = m_cueList->at(cueIndex);
    result.cueId = cue.id();
    result.cueNumber = cue.number();
    result.cueName = cue.name();

    // validate cue first
    if (m_validator) {
        ValidationResult validation = m_validator->validate(cue, m_cueList);
        result.validationPassed = validation.valid;
        for (const auto& issue : validation.issues) {
            if (issue.isError()) {
                result.validationErrors.append(issue.message);
            } else {
                result.warnings.append(issue.message);
            }
        }

        if (!validation.valid) {
            emit dryRunCompleted(cueIndex, result);
            return result;
        }
    }

    // simulate execution based on type
    QJsonObject currentState = m_initialState;
    qint64 currentTime = 0;

    switch (cue.type()) {
    case CueType::Snapshot: {
        // instant change to target parameters
        QJsonObject targetParams = cue.parameters();
        for (auto it = targetParams.begin(); it != targetParams.end(); ++it) {
            currentState[it.key()] = it.value();
        }
        result.timeline.append(qMakePair(currentTime, currentState));
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::Fade: {
        double fadeTime = cue.fadeTime();
        if (fadeTime <= 0) {
            // instant fade
            QJsonObject targetParams = cue.parameters();
            for (auto it = targetParams.begin(); it != targetParams.end(); ++it) {
                currentState[it.key()] = it.value();
            }
            result.timeline.append(qMakePair(currentTime, currentState));
        } else {
            // simulate fade timeline
            QJsonObject endState = currentState;
            QJsonObject targetParams = cue.parameters();
            for (auto it = targetParams.begin(); it != targetParams.end(); ++it) {
                endState[it.key()] = it.value();
            }

            result.timeline = simulateFade(currentState, endState, fadeTime, cue.fadeCurve());
            currentState = endState;
            result.totalDuration = fadeTime;
        }
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::Macro: {
        // expand macro & track child cues
        QSet<QString> visited;
        result.macroExpansion = expandMacro(cue.id(), visited);

        if (result.macroExpansion.isEmpty() && cue.childCueIds().isEmpty()) {
            result.warnings.append("Macro cue has no child cues");
        }

        // sum up all child fade times
        // (actual timing depends on execution mode)
        double totalChildDuration = 0.0;
        for (const QString& childId : result.macroExpansion) {
            const Cue* childCue = m_cueList->findById(childId);
            if (childCue && childCue->type() == CueType::Fade) {
                if (cue.macroExecutionMode() == MacroExecutionMode::Sequential) {
                    totalChildDuration += childCue->fadeTime();
                } else {
                    // parallel: total duration is max of all children
                    totalChildDuration = qMax(totalChildDuration, childCue->fadeTime());
                }
            }
        }
        result.totalDuration = totalChildDuration;
        result.finalState = currentState; // would need recursive simulation for accurate result
        result.wouldSucceed = !result.macroExpansion.isEmpty() || !cue.childCueIds().isEmpty();
        break;
    }

    case CueType::Wait: {
        // just a delay
        result.totalDuration = cue.autoFollowDelay();
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::Stop: {
        // apply parameters
        QJsonObject targetParams = cue.parameters();
        for (auto it = targetParams.begin(); it != targetParams.end(); ++it) {
            currentState[it.key()] = it.value();
        }
        result.timeline.append(qMakePair(currentTime, currentState));
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::GoTo: {
        // just navigates, no mixer changes
        result.finalState = currentState;
        result.wouldSucceed = true;
        result.warnings.append("GoTo cue - no mixer changes simulated");
        break;
    }
    }

    // add auto-follow delay to duration
    if (cue.autoFollow()) {
        result.totalDuration += cue.autoFollowDelay();
    }

    emit dryRunCompleted(cueIndex, result);
    return result;
}

DryRunResult DryRunEngine::executeDryRunById(const QString& cueId) {
    if (!m_cueList) {
        DryRunResult result;
        result.warnings.append("No cue list set");
        return result;
    }

    int index = m_cueList->indexOf(cueId);
    return executeDryRun(index);
}

QVector<DryRunResult> DryRunEngine::executeDryRunSequence(int startIndex, int maxCues) {
    QVector<DryRunResult> results;

    if (!m_cueList || startIndex < 0) {
        return results;
    }

    int currentIndex = startIndex;
    QJsonObject currentState = m_initialState;
    int cueCount = 0;

    while (currentIndex >= 0 && currentIndex < m_cueList->count() && cueCount < maxCues) {
        // set the current state for this cue's dry run
        setInitialState(currentState);

        DryRunResult result = executeDryRun(currentIndex);
        results.append(result);

        // update current state for next cue
        currentState = result.finalState;

        // check for auto-follow
        const Cue& cue = m_cueList->at(currentIndex);
        if (!cue.autoFollow()) {
            break; // no auto-follow, sequence ends
        }

        // move to next cue
        currentIndex++;
        cueCount++;
    }

    emit sequenceCompleted(results);
    return results;
}

QJsonObject DryRunEngine::predictedStateAtTime(int cueIndex, double secondsAfterStart) {
    if (!m_cueList || cueIndex < 0 || cueIndex >= m_cueList->count()) {
        return QJsonObject();
    }

    DryRunResult result = executeDryRun(cueIndex);

    if (result.timeline.isEmpty()) {
        return result.finalState;
    }

    qint64 targetTime = static_cast<qint64>(secondsAfterStart * 1000);

    // find state at the requested time
    QJsonObject state = m_initialState;
    for (const auto& entry : result.timeline) {
        if (entry.first <= targetTime) {
            state = entry.second;
        } else {
            break;
        }
    }

    return state;
}

QJsonObject DryRunEngine::predictedFinalState(int cueIndex) {
    DryRunResult result = executeDryRun(cueIndex);
    return result.finalState;
}

QVector<QPair<qint64, QJsonObject>> DryRunEngine::simulateFade(const QJsonObject& startState,
                                                               const QJsonObject& endState,
                                                               double durationSec, FadeCurve curve,
                                                               int samplesPerSecond) {
    QVector<QPair<qint64, QJsonObject>> timeline;

    int numSamples = qMax(1, static_cast<int>(durationSec * samplesPerSecond));
    double intervalMs = (durationSec * 1000.0) / numSamples;

    // add initial state
    timeline.append(qMakePair(qint64(0), startState));

    // generate intermediate states
    for (int i = 1; i <= numSamples; ++i) {
        double progress = static_cast<double>(i) / numSamples;
        qint64 time = static_cast<qint64>(i * intervalMs);

        // apply fade curve to get curved progress
        double curvedProgress = interpolate(progress, curve);

        QJsonObject state = startState;

        // interpolate each parameter
        for (auto it = endState.begin(); it != endState.end(); ++it) {
            QString path = it.key();
            QJsonValue endVal = it.value();

            if (endVal.isDouble()) {
                double start = startState.contains(path) ? startState[path].toDouble() : 0.0;
                double end = endVal.toDouble();
                double current = start + (end - start) * curvedProgress;
                state[path] = current;
            } else {
                // non-numeric values switch at midpoint
                if (progress >= 0.5) {
                    state[path] = endVal;
                }
            }
        }

        timeline.append(qMakePair(time, state));
    }

    return timeline;
}

QStringList DryRunEngine::expandMacro(const QString& cueId, QSet<QString>& visited) {
    QStringList expansion;

    if (!m_cueList || visited.contains(cueId)) {
        return expansion; // prevent infinite recursion
    }

    visited.insert(cueId);

    const Cue* cue = m_cueList->findById(cueId);
    if (!cue) {
        return expansion;
    }

    if (cue->type() != CueType::Macro) {
        // not a macro, just return this cue
        expansion.append(cueId);
        return expansion;
    }

    // expand children
    for (const QString& childId : cue->childCueIds()) {
        QStringList childExpansion = expandMacro(childId, visited);
        expansion.append(childExpansion);
    }

    return expansion;
}

double DryRunEngine::interpolate(double progress, FadeCurve curve) {
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
        // exponential curve
        // (exp(progress * 3) - 1) / (exp(3) - 1)
        return (std::exp(progress * 3.0) - 1.0) / (std::exp(3.0) - 1.0);

    default:
        return progress;
    }
}

} // namespace StageBlend
