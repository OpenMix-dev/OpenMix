#pragma once

#include "Cue.h"
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

namespace OpenMix {

class CueList;
class CueValidator;
struct ValidationResult;

struct DryRunResult {
    QString cueId;
    double cueNumber;
    QString cueName;

    // timeline of parameter changes: timestamp (ms from start) -> parameters
    QVector<QPair<qint64, QJsonObject>> timeline;

    // macro expansion order (if macro cue)
    QStringList macroExpansion;

    // total duration of auto-follows
    double totalDuration;

    // whether the cue would execute successfully
    bool wouldSucceed;

    // any warnings or issues detected
    QStringList warnings;

    // validation result
    bool validationPassed;
    QStringList validationErrors;

    // final state after execution completes
    QJsonObject finalState;
};

class DryRunEngine : public QObject {
    Q_OBJECT

  public:
    explicit DryRunEngine(QObject* parent = nullptr);

    void setCueList(CueList* cueList);
    void setValidator(CueValidator* validator);
    void setInitialState(const QJsonObject& state);
    QJsonObject initialState() const { return m_initialState; }

    DryRunResult executeDryRun(int cueIndex);
    DryRunResult executeDryRunById(const QString& cueId);
    QVector<DryRunResult> executeDryRunSequence(int startIndex, int maxCues = 100);
    QJsonObject predictedStateAtTime(int cueIndex, double secondsAfterStart);
    QJsonObject predictedFinalState(int cueIndex);

  signals:
    void dryRunStarted(int cueIndex);
    void dryRunCompleted(int cueIndex, const DryRunResult& result);
    void sequenceCompleted(const QVector<DryRunResult>& results);

  private:
    QStringList expandMacro(const QString& cueId, QSet<QString>& visited);

    CueList* m_cueList = nullptr;
    CueValidator* m_validator = nullptr;
    QJsonObject m_initialState;
};

} // namespace OpenMix
