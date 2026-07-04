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
    int channelNames = 0; // actor names taken from default profile rows
    QStringList profileSlots;
};

// Imports a .tmix show file (a SQLite database) into a Show.
// The show is cleared first, then populated from the file's config, actors,
// profiles, positions, ensembles and cues tables. These files store each
// channel's Show Setup name as that channel's default profile row, so default
// profiles become actor names and only additional profiles become voice
// slots. Best-effort: missing tables or columns are skipped rather than
// treated as errors.
class TmixImporter {
  public:
    // Populate `show` from the .tmix at `path`. Returns false and sets *error
    // only when the database cannot be opened.
    bool import(const QString& path, Show* show, QString* error = nullptr,
                TmixImportSummary* summary = nullptr);
};

} // namespace OpenMix
