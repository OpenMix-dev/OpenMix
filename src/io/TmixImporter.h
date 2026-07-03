#pragma once

#include <QString>
#include <QStringList>

namespace OpenMix {

class Show;

// What an import produced, for the post-import summary dialog.
struct TmixImportSummary {
    int actors = 0;
    int cues = 0;
    int positions = 0;
    int ensembles = 0;
    int rolesInferred = 0; // actor roles guessed from cue DCA labels
    QStringList profileSlots;
};

// Imports a .tmix show file (a SQLite database) into a Show.
// The show is cleared first, then populated from the file's config, actors,
// profiles, positions, ensembles and cues tables. Best-effort: missing tables
// or columns are skipped rather than treated as errors.
class TmixImporter {
  public:
    // Populate `show` from the .tmix at `path`. Returns false and sets *error
    // only when the database cannot be opened.
    bool import(const QString& path, Show* show, QString* error = nullptr,
                TmixImportSummary* summary = nullptr);
};

} // namespace OpenMix
