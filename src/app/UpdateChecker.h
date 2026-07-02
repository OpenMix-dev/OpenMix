#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace OpenMix {

// Checks GitHub Releases for a newer version than the running app. Compares the
// latest release tag (vMAJOR.MINOR.PATCH) against QCoreApplication version.
class UpdateChecker : public QObject {
    Q_OBJECT

  public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // GET the latest release from the GitHub API and emit one of the signals
    void checkForUpdates();

    // true if 'latest' is a higher version than 'current' (leading 'v' ignored)
    [[nodiscard]] static bool isNewer(const QString& latest, const QString& current);

  signals:
    void updateAvailable(const QString& version, const QString& url);
    void upToDate();
    void checkFailed(const QString& error);

  private:
    QNetworkAccessManager* m_net;
};

} // namespace OpenMix
