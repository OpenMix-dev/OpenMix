#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Parsing helpers for free-text cast entry: a comma-separated roles field and
// the multi-line "Add Multiple Actors" paste format (one actor per line, with
// optional tab-separated role cells as produced by pasting Name | Role columns
// from a spreadsheet).
//
// Header-only and free of any widget/Qt-GUI dependency so it links into
// headless unit tests.
namespace OpenMix::CastTextParse {

// Split "Cosette, Singer #1" into trimmed, non-empty, case-insensitively
// deduplicated role names.
[[nodiscard]] inline QStringList parseRoles(const QString& text) {
    QStringList roles;
    const QStringList tokens = text.split(QLatin1Char(','));
    for (const QString& token : tokens) {
        const QString role = token.trimmed();
        if (!role.isEmpty() && !roles.contains(role, Qt::CaseInsensitive))
            roles.append(role);
    }
    return roles;
}

// Append `added` roles onto `existing`, skipping case-insensitive duplicates.
[[nodiscard]] inline QStringList mergeRoles(const QStringList& existing,
                                            const QStringList& added) {
    QStringList merged = existing;
    for (const QString& role : added) {
        if (!merged.contains(role, Qt::CaseInsensitive))
            merged.append(role);
    }
    return merged;
}

struct CastLine {
    QString name;
    QStringList roles;
};

// One actor per line; the first tab-separated cell is the name, any further
// cells are comma-split into roles. Blank lines (and lines with an empty name
// cell) are skipped. Trimming absorbs \r from \r\n input.
[[nodiscard]] inline QList<CastLine> parseCastLines(const QString& text) {
    QList<CastLine> lines;
    const QStringList rawLines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : rawLines) {
        const QStringList cells = rawLine.split(QLatin1Char('\t'));
        CastLine line;
        line.name = cells.first().trimmed();
        if (line.name.isEmpty())
            continue;
        for (int i = 1; i < cells.size(); ++i) {
            for (const QString& role : parseRoles(cells[i])) {
                if (!line.roles.contains(role, Qt::CaseInsensitive))
                    line.roles.append(role);
            }
        }
        lines.append(line);
    }
    return lines;
}

} // namespace OpenMix::CastTextParse
