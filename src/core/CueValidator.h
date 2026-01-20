#pragma once

#include <QList>
#include <QObject>
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
        for (const auto& issue : issues) {
            if (issue.isError())
                return true;
        }
        return false;
    }

    bool hasWarnings() const {
        for (const auto& issue : issues) {
            if (issue.isWarning())
                return true;
        }
        return false;
    }

    int errorCount() const {
        int count = 0;
        for (const auto& issue : issues) {
            if (issue.isError())
                count++;
        }
        return count;
    }

    int warningCount() const {
        int count = 0;
        for (const auto& issue : issues) {
            if (issue.isWarning())
                count++;
        }
        return count;
    }
};

class CueValidator : public QObject {
    Q_OBJECT

  public:
    explicit CueValidator(QObject* parent = nullptr);

    ValidationResult validate(const Cue& cue, const CueList* cueList) const;
    ValidationResult validateAll(const CueList* cueList) const;

    bool validateMacroIds(const Cue& cue, const CueList* cueList,
                          QList<ValidationIssue>& issues) const;
    bool validateParameters(const Cue& cue, QList<ValidationIssue>& issues) const;
    bool detectCircularMacroReferences(const Cue& cue, const CueList* cueList,
                                       QList<ValidationIssue>& issues) const;
    bool detectConflictingFadeTargets(const Cue& cue, const CueList* cueList,
                                      QList<ValidationIssue>& issues) const;

  signals:
    void validationCompleted(const ValidationResult& result);

  private:
    bool hasCircularReference(const QString& cueId, const CueList* cueList, QSet<QString>& visited,
                              QSet<QString>& recursionStack) const;
    QSet<QString> collectMacroParameters(const Cue& cue, const CueList* cueList) const;
};

} // namespace OpenMix
