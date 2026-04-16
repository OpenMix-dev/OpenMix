#pragma once

#include <QList>
#include <QObject>
#include <algorithm>
#include <QSet>
#include <QString>

namespace OpenMix {

class Cue;
class CueList;

struct ValidationIssue {
    enum Severity { Warning, Error };
    Severity severity;
    QString message;
    QString parameterPath;

    bool isError() const { return severity == Error; }
    bool isWarning() const { return severity == Warning; }
};

struct ValidationResult {
    bool valid; // true if no errors (warnings are okay)
    QList<ValidationIssue> issues;

    bool hasErrors() const {
        return std::any_of(issues.begin(), issues.end(),
                           [](const ValidationIssue& i) { return i.isError(); });
    }

    bool hasWarnings() const {
        return std::any_of(issues.begin(), issues.end(),
                           [](const ValidationIssue& i) { return i.isWarning(); });
    }

    int errorCount() const {
        return static_cast<int>(std::count_if(issues.begin(), issues.end(),
                                              [](const ValidationIssue& i) { return i.isError(); }));
    }

    int warningCount() const {
        return static_cast<int>(std::count_if(issues.begin(), issues.end(),
                                              [](const ValidationIssue& i) { return i.isWarning(); }));
    }
};

class CueValidator : public QObject {
    Q_OBJECT

  public:
    explicit CueValidator(QObject* parent = nullptr);

    ValidationResult validate(const Cue& cue, const CueList* cueList);
    ValidationResult validateAll(const CueList* cueList);

  signals:
    void validationCompleted(const ValidationResult& result);

  private:
    bool validateMacroIds(const Cue& cue, const CueList* cueList,
                          QList<ValidationIssue>& issues) const;
    bool validateParameters(const Cue& cue, QList<ValidationIssue>& issues) const;
    bool detectCircularMacroReferences(const Cue& cue, const CueList* cueList,
                                       QList<ValidationIssue>& issues) const;
    bool detectConflictingFadeTargets(const Cue& cue, const CueList* cueList,
                                      QList<ValidationIssue>& issues) const;
    bool hasCircularReference(const QString& cueId, const CueList* cueList, QSet<QString>& visited,
                              QSet<QString>& recursionStack) const;
    QSet<QString> collectMacroParameters(const Cue& cue, const CueList* cueList) const;
};

} // namespace OpenMix
