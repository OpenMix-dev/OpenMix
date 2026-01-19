#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

namespace StageBlend {

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

    // total duration of all fades & auto-follows
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

    // set the cue list to operate on
    void setCueList(CueList* cueList);

    // set the validator for pre-execution checks
    void setValidator(CueValidator* validator);

    // set the initial mixer state (for calculating fade start values)
    void setInitialState(const QJsonObject& state);
    QJsonObject initialState() const { return m_initialState; }

    // execute dry run of a single cue
    DryRunResult executeDryRun(int cueIndex);
    DryRunResult executeDryRunById(const QString& cueId);

    // execute dry run of a sequence of cues (following auto-follows)
    QVector<DryRunResult> executeDryRunSequence(int startIndex, int maxCues = 100);

    // predict the mixer state at a specific time after cue start
    QJsonObject predictedStateAtTime(int cueIndex, double secondsAfterStart);

    // get the predicted final state after a cue completes
    QJsonObject predictedFinalState(int cueIndex);

  signals:
    void dryRunStarted(int cueIndex);
    void dryRunCompleted(int cueIndex, const DryRunResult& result);
    void sequenceCompleted(const QVector<DryRunResult>& results);

  private:
    // simulate a fade & return timeline
    QVector<QPair<qint64, QJsonObject>> simulateFade(const QJsonObject& startState,
                                                     const QJsonObject& endState,
                                                     double durationSec, int samplesPerSecond = 10);

    // expand macro cue recursively
    QStringList expandMacro(const QString& cueId, QSet<QString>& visited);

    // calculate fade interpolation
    static double interpolate(double progress, int curveType);

    CueList* m_cueList = nullptr;
    CueValidator* m_validator = nullptr;
    QJsonObject m_initialState;
};

} // namespace StageBlend
