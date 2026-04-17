#include "DryRunEngine.h"
#include "Cue.h"
#include "CueList.h"
#include "CueValidator.h"
#include <QSet>

namespace OpenMix {

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

    QJsonObject currentState = m_initialState;
    qint64 currentTime = 0;

    switch (cue.type()) {
    case CueType::Snapshot: {
        QJsonObject targetParams = cue.parameters();
        for (const auto& [path, value] : targetParams.asKeyValueRange()) {
            currentState[path.toString()] = value;
        }
        result.timeline.append({currentTime, currentState});
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::Macro: {
        QSet<QString> visited;
        result.macroExpansion = expandMacro(cue.id(), visited);

        if (result.macroExpansion.isEmpty() && cue.childCueIds().isEmpty()) {
            result.warnings.append("Macro cue has no child cues");
        }

        result.finalState = currentState;
        result.wouldSucceed = !result.macroExpansion.isEmpty() || !cue.childCueIds().isEmpty();
        break;
    }

    case CueType::Wait: {
        result.totalDuration = cue.autoFollowDelay();
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::Stop: {
        for (const auto& [path, value] : cue.parameters().asKeyValueRange()) {
            currentState[path.toString()] = value;
        }
        result.timeline.append({currentTime, currentState});
        result.finalState = currentState;
        result.wouldSucceed = true;
        break;
    }

    case CueType::GoTo: {
        result.finalState = currentState;
        result.wouldSucceed = true;
        result.warnings.append("GoTo cue - no mixer changes simulated");
        break;
    }
    }

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

    return executeDryRun(m_cueList->indexOf(cueId).value_or(-1));
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
        setInitialState(currentState);

        DryRunResult result = executeDryRun(currentIndex);
        results.append(result);

        currentState = result.finalState;

        const Cue& cue = m_cueList->at(currentIndex);
        if (!cue.autoFollow()) {
            break;
        }

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

    QJsonObject state = m_initialState;
    for (const auto& [time, snapshot] : result.timeline) {
        if (time <= targetTime) {
            state = snapshot;
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

QStringList DryRunEngine::expandMacro(const QString& cueId, QSet<QString>& visited) {
    QStringList expansion;

    if (!m_cueList || visited.contains(cueId)) {
        return expansion;
    }

    visited.insert(cueId);

    const Cue* cue = m_cueList->findById(cueId);
    if (!cue) {
        return expansion;
    }

    if (cue->type() != CueType::Macro) {
        expansion.append(cueId);
        return expansion;
    }

    for (const QString& childId : cue->childCueIds()) {
        QStringList childExpansion = expandMacro(childId, visited);
        expansion.append(childExpansion);
    }

    return expansion;
}

} // namespace OpenMix
