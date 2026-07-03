#include "AutoUpdater.h"

#if defined(Q_OS_MACOS)
// implemented in AutoUpdaterSparkle.mm
extern "C" void openmix_sparkle_init();
extern "C" void openmix_sparkle_check_with_ui();
extern "C" void openmix_sparkle_cleanup();
#else
#include "UpdateChecker.h"
#include "ui/UpdateDialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#endif

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace OpenMix {

#if !defined(Q_OS_MACOS)
namespace {
constexpr const char* kLastCheckKey = "Updates/lastCheckTime";
constexpr const char* kSkippedVersionKey = "Updates/skippedVersion";
constexpr int kCheckIntervalHours = 24;
} // namespace
#endif

AutoUpdater::AutoUpdater(QObject* parent) : QObject(parent) {}

AutoUpdater::~AutoUpdater() {
#if defined(Q_OS_MACOS)
    openmix_sparkle_cleanup();
#endif
}

void AutoUpdater::initialize(QWidget* dialogParent) {
#if defined(Q_OS_MACOS)
    // Sparkle reads SUFeedURL / SUPublicEDKey / SUEnableAutomaticChecks from the
    // app's Info.plist; the controller starts the background updater here.
    Q_UNUSED(dialogParent);
    openmix_sparkle_init();
#else
    m_dialogParent = dialogParent;

    // check shortly after startup, then hourly against the daily deadline (a
    // catch-up timer survives sleep/suspend better than one long 24h timer)
    QTimer::singleShot(10 * 1000, this, &AutoUpdater::maybeScheduledCheck);
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AutoUpdater::maybeScheduledCheck);
    timer->start(60 * 60 * 1000);
#endif
}

void AutoUpdater::checkForUpdatesNow() {
#if defined(Q_OS_MACOS)
    openmix_sparkle_check_with_ui();
#else
    runCheck(/*manual=*/true);
#endif
}

#if !defined(Q_OS_MACOS)

void AutoUpdater::maybeScheduledCheck() {
    const QDateTime lastCheck = QSettings().value(kLastCheckKey).toDateTime();
    if (lastCheck.isValid() &&
        lastCheck.secsTo(QDateTime::currentDateTimeUtc()) < kCheckIntervalHours * 60 * 60)
        return;
    runCheck(/*manual=*/false);
}

void AutoUpdater::runCheck(bool manual) {
    if (m_dialog) { // a prompt is already up; don't stack another check on it
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }

    auto* checker = new UpdateChecker(this);
    const auto stamp = []() {
        QSettings().setValue(kLastCheckKey, QDateTime::currentDateTimeUtc());
    };

    connect(checker, &UpdateChecker::updateAvailable, this,
            [this, checker, manual, stamp](const UpdateInfo& info) {
                checker->deleteLater();
                stamp();
                if (!manual && info.version == QSettings().value(kSkippedVersionKey).toString())
                    return;
                showUpdateDialog(info);
            });
    connect(checker, &UpdateChecker::upToDate, this, [this, checker, manual, stamp]() {
        checker->deleteLater();
        stamp();
        if (manual)
            QMessageBox::information(m_dialogParent, tr("Up to Date"),
                                     tr("You are running the latest version (%1).")
                                         .arg(QCoreApplication::applicationVersion()));
    });
    connect(checker, &UpdateChecker::checkFailed, this,
            [this, checker, manual, stamp](const QString& error) {
                checker->deleteLater();
                stamp();
                if (manual)
                    QMessageBox::warning(m_dialogParent, tr("Update Check Failed"),
                                         tr("Could not check for updates:\n%1").arg(error));
                else
                    qWarning() << "scheduled update check failed:" << error;
            });
    checker->checkForUpdates();
}

void AutoUpdater::showUpdateDialog(const UpdateInfo& info) {
    m_dialog = new UpdateDialog(info, m_dialogParent);
    connect(m_dialog, &UpdateDialog::skipVersionRequested, this, [](const QString& version) {
        QSettings().setValue(kSkippedVersionKey, version);
    });
    connect(m_dialog, &UpdateDialog::readyToInstall, this, &AutoUpdater::launchInstallerAndQuit);
    m_dialog->show(); // non-modal: never block a running show
}

void AutoUpdater::launchInstallerAndQuit(const QString& installerPath) {
    // close the main window first so the unsaved-show prompt runs before the
    // installer launches; on cancel the dialog stays ready for another attempt
    if (m_dialogParent && !m_dialogParent->close())
        return;

#if defined(Q_OS_WIN)
    // the running exe is locked by Windows, so the app must fully exit before
    // NSIS copies files (the 2s delay); "start" is required so the elevation
    // prompt works, and the app relaunches even if the install fails
    const QString installer = QDir::toNativeSeparators(installerPath);
    const QString app = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QProcess proc;
    proc.setProgram("cmd.exe");
    proc.setNativeArguments(QString("/c timeout /t 2 /nobreak >nul & "
                                    "start \"\" /wait \"%1\" /S & start \"\" \"%2\"")
                                .arg(installer, app));
    proc.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
    proc.startDetached();
#else
    qInfo() << "test mode: would quit and run installer" << installerPath;
#endif
    QCoreApplication::quit();
}

#endif // !Q_OS_MACOS

} // namespace OpenMix
