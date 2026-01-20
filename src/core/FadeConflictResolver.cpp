#include "FadeConflictResolver.h"
#include "Cue.h"
#include "CueList.h"
#include "PlaybackEngine.h"

namespace OpenMix {

FadeConflictResolver::FadeConflictResolver(QObject* parent) : QObject(parent) {}

void FadeConflictResolver::setCueList(CueList* cueList) { m_cueList = cueList; }

void FadeConflictResolver::setResolutionPolicy(ConflictResolution policy) { m_policy = policy; }

QJsonObject FadeConflictResolver::resolveConflicts(const QVector<FadeInstance>& activeFades) {
    m_lastConflicts.clear();

    if (activeFades.isEmpty()) {
        return QJsonObject();
    }

    // collect all parameters from all active fades
    // map: parameter path -> list of (cueId, value) pairs
    QMap<QString, QVector<QPair<QString, QVariant>>> parameterSources;

    for (const FadeInstance& fade : activeFades) {
        QJsonObject values =
            const_cast<FadeInstance&>(fade).update(QDateTime::currentMSecsSinceEpoch());
        QString cueId = fade.cueId();

        for (auto it = values.begin(); it != values.end(); ++it) {
            QString path = it.key();
            QVariant value = it.value().toVariant();
            parameterSources[path].append(qMakePair(cueId, value));
        }
    }

    // resolve conflicts for each parameter
    QJsonObject resolvedValues;

    for (auto it = parameterSources.begin(); it != parameterSources.end(); ++it) {
        QString path = it.key();
        QVector<QPair<QString, QVariant>>& sources = it.value();

        if (sources.size() == 1) {
            // no conflict, use the single value
            resolvedValues[path] = QJsonValue::fromVariant(sources.first().second);
        } else {
            // conflict detected, resolve based on policy
            FadeConflict conflict;
            conflict.parameterPath = path;

            for (const auto& source : sources) {
                conflict.conflictingCueIds.append(source.first);
            }

            emit conflictDetected(conflict);

            // select the winning cue based on policy
            QString winningCueId = selectWinningFade(conflict.conflictingCueIds, m_policy);

            // find value from winning cue
            QVariant winningValue;
            for (const auto& source : sources) {
                if (source.first == winningCueId) {
                    winningValue = source.second;
                    break;
                }
            }

            conflict.resolvedToCueId = winningCueId;
            conflict.resolvedValue = winningValue;

            resolvedValues[path] = QJsonValue::fromVariant(winningValue);

            m_lastConflicts.append(conflict);
            emit conflictResolved(conflict);
        }
    }

    return resolvedValues;
}

QList<FadeConflict>
FadeConflictResolver::detectPotentialConflicts(const QVector<FadeInstance>& activeFades) {
    QList<FadeConflict> conflicts;

    if (activeFades.size() < 2) {
        return conflicts;
    }

    // collect all parameters from all active fades
    QMap<QString, QStringList> parameterToCues;

    for (const FadeInstance& fade : activeFades) {
        // get end parameters (target values) to check for potential conflicts
        QJsonObject values =
            const_cast<FadeInstance&>(fade).update(QDateTime::currentMSecsSinceEpoch());
        QString cueId = fade.cueId();

        for (auto it = values.begin(); it != values.end(); ++it) {
            QString path = it.key();
            parameterToCues[path].append(cueId);
        }
    }

    // find conflicts (parameters controlled by multiple fades)
    for (auto it = parameterToCues.begin(); it != parameterToCues.end(); ++it) {
        if (it.value().size() > 1) {
            FadeConflict conflict;
            conflict.parameterPath = it.key();
            conflict.conflictingCueIds = it.value();
            conflicts.append(conflict);
        }
    }

    return conflicts;
}

QStringList FadeConflictResolver::conflictingParameters(const QVector<FadeInstance>& activeFades) {
    QList<FadeConflict> conflicts = detectPotentialConflicts(activeFades);
    QStringList params;
    for (const FadeConflict& conflict : conflicts) {
        params.append(conflict.parameterPath);
    }
    return params;
}

int FadeConflictResolver::getCuePriority(const QString& cueId) const {
    if (!m_cueList)
        return 0;

    const Cue* cue = m_cueList->findById(cueId);
    if (!cue)
        return 0;

    // doesn't currently have a priority field, so use cue number as proxy
    // lower cue number = higher priority
    return static_cast<int>(cue->number() * 100);
}

double FadeConflictResolver::getCueNumber(const QString& cueId) const {
    if (!m_cueList)
        return 0.0;

    const Cue* cue = m_cueList->findById(cueId);
    if (!cue)
        return 0.0;

    return cue->number();
}

QString FadeConflictResolver::selectWinningFade(const QStringList& cueIds,
                                                ConflictResolution policy) const {
    if (cueIds.isEmpty())
        return QString();
    if (cueIds.size() == 1)
        return cueIds.first();

    switch (policy) {
    case ConflictResolution::LastWins:
        // last cue in the list wins (most recently started fade)
        return cueIds.last();

    case ConflictResolution::FirstWins:
        // first cue in the list wins (first started fade)
        return cueIds.first();

    case ConflictResolution::HigherPriority:
    case ConflictResolution::CueOrder: {
        // find cue w/ lowest number (highest priority)
        QString winner = cueIds.first();
        double lowestNumber = getCueNumber(winner);

        for (const QString& cueId : cueIds) {
            double num = getCueNumber(cueId);
            if (num < lowestNumber) {
                lowestNumber = num;
                winner = cueId;
            }
        }
        return winner;
    }
    }

    return cueIds.last(); // default fallback
}

} // namespace OpenMix
