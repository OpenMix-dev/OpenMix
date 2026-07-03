#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace OpenMix {

namespace {
// small JSON published by CI: {"tag_name":"vX.Y.Z","html_url":"...",
// "windows_installer_url":"...","windows_installer_sha256":"..."}
constexpr const char* kLatestReleaseApi = "https://openmix.dev/latest.json";

QString feedUrl() {
    // override for local testing against a fake feed
    const QString env = qEnvironmentVariable("OPENMIX_UPDATE_FEED_URL");
    return env.isEmpty() ? QString::fromLatin1(kLatestReleaseApi) : env;
}
} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

bool UpdateChecker::isNewer(const QString& latest, const QString& current) {
    auto parse = [](QString v) {
        v.remove('v');
        QList<int> out;
        for (const QString& part : v.split('.'))
            out.append(part.toInt());
        return out;
    };
    const QList<int> a = parse(latest);
    const QList<int> b = parse(current);
    for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int x = i < a.size() ? a[i] : 0;
        const int y = i < b.size() ? b[i] : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

void UpdateChecker::checkForUpdates() {
    QNetworkRequest req{QUrl(feedUrl())};
    req.setHeader(QNetworkRequest::UserAgentHeader, "OpenMix");
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = obj.value("tag_name").toString();
        if (tag.isEmpty()) {
            emit checkFailed(tr("No releases found."));
            return;
        }

        UpdateInfo info;
        info.version = tag;
        info.pageUrl = obj.value("html_url").toString();
        if (info.pageUrl.isEmpty())
            info.pageUrl = "https://openmix.dev/download";
        info.installerUrl = obj.value("windows_installer_url").toString();
        info.sha256 = obj.value("windows_installer_sha256").toString().toLower();

        if (isNewer(tag, QCoreApplication::applicationVersion()))
            emit updateAvailable(info);
        else
            emit upToDate();
    });
}

} // namespace OpenMix
