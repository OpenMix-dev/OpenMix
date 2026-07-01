#pragma once

#include <QString>

namespace OpenMix {

class Show;

// Imports a TheatreMix-format .tmix show file (a SQLite database) into a Show.
// The show is cleared first, then populated from the file's config, actors,
// profiles, positions, ensembles and cues tables. Best-effort: missing tables
// or columns are skipped rather than treated as errors.
class TmixImporter {
  public:
    // Populate `show` from the .tmix at `path`. Returns false and sets *error
    // only when the database cannot be opened.
    bool import(const QString& path, Show* show, QString* error = nullptr);
};

} // namespace OpenMix
