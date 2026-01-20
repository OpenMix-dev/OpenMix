#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

namespace OpenMix {

class CueList;
class FadeInstance;

enum class ConflictResolution {
    LastWins,       // most recent fade takes precedence (current)
    FirstWins,      // first fade locks the parameter
    HigherPriority, // use cue priority field (lower number = higher priority)
    CueOrder        // lower cue number wins
};

struct FadeConflict {
    QString parameterPath;
    QStringList conflictingCueIds;
    QString resolvedToCueId;
    QVariant resolvedValue;
};

class FadeConflictResolver : public QObject {
    Q_OBJECT

  public:
    explicit FadeConflictResolver(QObject* parent = nullptr);

    // set the cue list for cue order/priority lookup
    void setCueList(CueList* cueList);

    // set the conflict resolution policy
    void setResolutionPolicy(ConflictResolution policy);
    ConflictResolution resolutionPolicy() const { return m_policy; }

    // resolve conflicts between multiple active fades
    // returns merged parameter values according to the resolution policy
    QJsonObject resolveConflicts(const QVector<FadeInstance>& activeFades);

    // detect potential conflicts without resolving (for warnings)
    QList<FadeConflict> detectPotentialConflicts(const QVector<FadeInstance>& activeFades);

    // get list of parameters currently in conflict
    QStringList conflictingParameters(const QVector<FadeInstance>& activeFades);

    // get the last resolved conflicts (for debugging/logging)
    QList<FadeConflict> lastResolvedConflicts() const { return m_lastConflicts; }

  signals:
    void conflictDetected(const FadeConflict& conflict);
    void conflictResolved(const FadeConflict& conflict);

  private:
    // get the priority of a cue for conflict resolution
    int getCuePriority(const QString& cueId) const;

    // get the cue number for conflict resolution
    double getCueNumber(const QString& cueId) const;

    // select winning fade based on policy
    QString selectWinningFade(const QStringList& cueIds, ConflictResolution policy) const;

    ConflictResolution m_policy = ConflictResolution::LastWins;
    CueList* m_cueList = nullptr;
    QList<FadeConflict> m_lastConflicts;
};

} // namespace OpenMix
