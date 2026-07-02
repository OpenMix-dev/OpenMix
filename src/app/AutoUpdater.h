#pragma once

#include <QObject>

namespace OpenMix {

// Cross-platform automatic updates.
//   - Windows: WinSparkle (silent background check, download, install, relaunch)
//   - macOS:   Sparkle    (same, via an Objective-C++ bridge)
//   - Linux/other: falls back to a GitHub-release check that notifies and links
//     to the download (no silent self-install; handled by MainWindow's checker)
//
// initialize() wires the framework and enables periodic background checks; call
// it once at startup. checkForUpdatesNow() runs a user-triggered check with UI.
class AutoUpdater : public QObject {
    Q_OBJECT

  public:
    explicit AutoUpdater(QObject* parent = nullptr);
    ~AutoUpdater() override;

    // true if a real self-updating framework is compiled in for this platform
    [[nodiscard]] static bool hasSilentUpdates();

    void initialize();
    void checkForUpdatesNow();

  signals:
    // emitted only on platforms without a silent updater, so the UI can fall
    // back to the notify-and-link checker
    void manualCheckRequested();
};

} // namespace OpenMix
