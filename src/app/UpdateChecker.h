#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace OpenMix {

// Everything the update UI needs about the newest release. installerUrl/sha256
// are only present when CI published a Windows installer for the release; when
// absent the UI degrades to linking the download page.
struct UpdateInfo {
    QString version;      // release tag, e.g. "v1.2.0"
    QString pageUrl;      // human download page
    QString installerUrl; // direct Windows installer download (may be empty)
    QString sha256;       // hex sha256 of the installer (may be empty)
};

// Checks the published release feed for a newer version than the running app.
// Compares the latest release tag (vMAJOR.MINOR.PATCH) against QCoreApplication
// version.
class UpdateChecker : public QObject {
    Q_OBJECT

  public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // GET the latest release from the feed and emit one of the signals
    void checkForUpdates();

    // true if 'latest' is a higher version than 'current' (leading 'v' ignored)
    [[nodiscard]] static bool isNewer(const QString& latest, const QString& current);

  signals:
    void updateAvailable(const OpenMix::UpdateInfo& info);
    void upToDate();
    void checkFailed(const QString& error);

  private:
    QNetworkAccessManager* m_net;
};

} // namespace OpenMix
