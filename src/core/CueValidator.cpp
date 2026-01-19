#include "CueValidator.h"
#include "Cue.h"
#include "CueList.h"
#include <QJsonObject>

namespace StageBlend {

CueValidator::CueValidator(QObject* parent) : QObject(parent) {}

ValidationResult CueValidator::validate(const Cue& cue, const CueList* cueList) const {
    ValidationResult result;
    result.valid = true;

    // validate macro child IDs exist
    if (cue.type() == CueType::Macro) {
        if (!validateMacroIds(cue, cueList, result.issues)) {
            result.valid = false;
        }

        // check for circular macro references
        if (!detectCircularMacroReferences(cue, cueList, result.issues)) {
            result.valid = false;
        }

        // check for conflicting fade targets in parallel macros
        if (cue.macroExecutionMode() == MacroExecutionMode::Parallel) {
            if (!detectConflictingFadeTargets(cue, cueList, result.issues)) {
                // warning, not error
            }
        }
    }

    // validate parameters
    if (!validateParameters(cue, result.issues)) {
        result.valid = false;
    }

    emit const_cast<CueValidator*>(this)->validationCompleted(result);
    return result;
}

ValidationResult CueValidator::validateAll(const CueList* cueList) const {
    ValidationResult result;
    result.valid = true;

    if (!cueList) {
        result.valid = false;
        result.issues.append({ValidationIssue::Error, tr("Cue list is null"), QString()});
        return result;
    }

    for (int i = 0; i < cueList->count(); ++i) {
        const Cue& cue = cueList->at(i);
        ValidationResult cueResult = validate(cue, cueList);

        // prefix issues with cue info
        for (ValidationIssue issue : cueResult.issues) {
            issue.message = tr("Cue %1 (%2): %3")
                                .arg(cue.number(), 0, 'f', 1)
                                .arg(cue.name().isEmpty() ? cue.id() : cue.name())
                                .arg(issue.message);
            result.issues.append(issue);
        }

        if (!cueResult.valid) {
            result.valid = false;
        }
    }

    emit const_cast<CueValidator*>(this)->validationCompleted(result);
    return result;
}

bool CueValidator::validateMacroIds(const Cue& cue, const CueList* cueList,
                                    QList<ValidationIssue>& issues) const {
    if (cue.type() != CueType::Macro) {
        return true;
    }

    bool valid = true;
    QStringList childIds = cue.childCueIds();

    if (childIds.isEmpty()) {
        issues.append({ValidationIssue::Warning, tr("Macro cue has no child cues"), QString()});
        // warning, not error
    }

    for (const QString& childId : childIds) {
        if (!cueList || !cueList->findById(childId)) {
            issues.append({ValidationIssue::Error,
                           tr("Macro references non-existent cue ID: %1").arg(childId), QString()});
            valid = false;
        }
    }

    return valid;
}

bool CueValidator::validateParameters(const Cue& cue, QList<ValidationIssue>& issues) const {
    bool valid = true;

    // fade & snapshot cues should have parameters
    if (cue.type() == CueType::Fade || cue.type() == CueType::Snapshot) {
        QJsonObject params = cue.parameters();
        if (params.isEmpty()) {
            issues.append({ValidationIssue::Warning,
                           tr("%1 cue has no parameters defined")
                               .arg(cue.type() == CueType::Fade ? tr("Fade") : tr("Snapshot")),
                           QString()});
            // warning, not error
        }
    }

    // fade cues should have positive fade time
    if (cue.type() == CueType::Fade && cue.fadeTime() <= 0) {
        issues.append(
            {ValidationIssue::Warning, tr("Fade cue has zero or negative fade time"), QString()});
    }

    // validate parameter paths (basic check for OSC-like format)
    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        QString path = it.key();
        if (!path.startsWith('/')) {
            issues.append({ValidationIssue::Warning,
                           tr("Parameter path '%1' does not start with '/'").arg(path), path});
        }
    }

    return valid;
}

bool CueValidator::detectCircularMacroReferences(const Cue& cue, const CueList* cueList,
                                                 QList<ValidationIssue>& issues) const {
    if (cue.type() != CueType::Macro || !cueList) {
        return true;
    }

    QSet<QString> visited;
    QSet<QString> recursionStack;

    if (hasCircularReference(cue.id(), cueList, visited, recursionStack)) {
        issues.append({ValidationIssue::Error, tr("Circular macro reference detected"), QString()});
        return false;
    }

    return true;
}

bool CueValidator::hasCircularReference(const QString& cueId, const CueList* cueList,
                                        QSet<QString>& visited,
                                        QSet<QString>& recursionStack) const {
    if (!cueList)
        return false;

    // if already in recursion stack, we found a cycle
    if (recursionStack.contains(cueId)) {
        return true;
    }

    // if already visited & not in recursion stack, no cycle through this path
    if (visited.contains(cueId)) {
        return false;
    }

    const Cue* cue = cueList->findById(cueId);
    if (!cue)
        return false;

    // only macros can create cycles
    if (cue->type() != CueType::Macro) {
        visited.insert(cueId);
        return false;
    }

    visited.insert(cueId);
    recursionStack.insert(cueId);

    // check all children
    for (const QString& childId : cue->childCueIds()) {
        if (hasCircularReference(childId, cueList, visited, recursionStack)) {
            return true;
        }
    }

    recursionStack.remove(cueId);
    return false;
}

bool CueValidator::detectConflictingFadeTargets(const Cue& cue, const CueList* cueList,
                                                QList<ValidationIssue>& issues) const {
    if (cue.type() != CueType::Macro || !cueList) {
        return true;
    }

    if (cue.macroExecutionMode() != MacroExecutionMode::Parallel) {
        return true;
    }

    // collect all parameters from parallel child cues
    QMap<QString, QStringList> parameterToCues; // parameter path -> list of cue names/ids

    for (const QString& childId : cue.childCueIds()) {
        const Cue* childCue = cueList->findById(childId);
        if (!childCue)
            continue;

        // get parameters from this child (including any nested macro children)
        QSet<QString> childParams = collectMacroParameters(*childCue, cueList);

        QString cueName = childCue->name().isEmpty()
                              ? QString("Cue %1").arg(childCue->number(), 0, 'f', 1)
                              : childCue->name();

        for (const QString& param : childParams) {
            parameterToCues[param].append(cueName);
        }
    }

    // check for conflicts (same parameter in multiple cues)
    bool hasConflicts = false;
    for (auto it = parameterToCues.begin(); it != parameterToCues.end(); ++it) {
        if (it.value().size() > 1) {
            issues.append({ValidationIssue::Warning,
                           tr("Parameter '%1' is targeted by multiple parallel cues: %2")
                               .arg(it.key())
                               .arg(it.value().join(", ")),
                           it.key()});
            hasConflicts = true;
        }
    }

    return !hasConflicts;
}

QSet<QString> CueValidator::collectMacroParameters(const Cue& cue, const CueList* cueList) const {
    QSet<QString> params;

    if (cue.type() == CueType::Macro && cueList) {
        // recursively collect from children
        for (const QString& childId : cue.childCueIds()) {
            const Cue* childCue = cueList->findById(childId);
            if (childCue) {
                params.unite(collectMacroParameters(*childCue, cueList));
            }
        }
    } else {
        // collect direct parameters
        QJsonObject cueParams = cue.parameters();
        for (auto it = cueParams.begin(); it != cueParams.end(); ++it) {
            params.insert(it.key());
        }
    }

    return params;
}

} // namespace StageBlend
