#include "AutoUpdater.h"

#include <QCoreApplication>

#if defined(Q_OS_WIN)
#include <winsparkle.h>
#elif defined(Q_OS_MACOS)
// implemented in AutoUpdaterSparkle.mm
extern "C" void openmix_sparkle_init();
extern "C" void openmix_sparkle_check_with_ui();
extern "C" void openmix_sparkle_cleanup();
#endif

namespace OpenMix {

namespace {
// Per-OS appcast feeds published by CI to GitHub Pages. Sparkle/WinSparkle poll
// these for new versions + signatures.
constexpr const char* kWindowsAppcast =
    "https://johnqherman.github.io/OpenMix/appcast-windows.xml";
} // namespace

AutoUpdater::AutoUpdater(QObject* parent) : QObject(parent) {}

AutoUpdater::~AutoUpdater() {
#if defined(Q_OS_WIN)
    win_sparkle_cleanup();
#elif defined(Q_OS_MACOS)
    openmix_sparkle_cleanup();
#endif
}

bool AutoUpdater::hasSilentUpdates() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

void AutoUpdater::initialize() {
#if defined(Q_OS_WIN)
    win_sparkle_set_appcast_url(kWindowsAppcast);
    const std::wstring version = QCoreApplication::applicationVersion().toStdWString();
    win_sparkle_set_app_details(L"OpenMix", L"OpenMix", version.c_str());
    win_sparkle_set_automatic_check_for_updates(1);
    win_sparkle_set_update_check_interval(24 * 60 * 60); // daily
    win_sparkle_init();
#elif defined(Q_OS_MACOS)
    // Sparkle reads SUFeedURL / SUPublicEDKey / SUEnableAutomaticChecks from the
    // app's Info.plist; the controller starts the background updater here.
    openmix_sparkle_init();
#endif
}

void AutoUpdater::checkForUpdatesNow() {
#if defined(Q_OS_WIN)
    win_sparkle_check_update_with_ui();
#elif defined(Q_OS_MACOS)
    openmix_sparkle_check_with_ui();
#else
    // no silent updater on this platform: let the UI run the notify-and-link check
    emit manualCheckRequested();
#endif
}

} // namespace OpenMix
