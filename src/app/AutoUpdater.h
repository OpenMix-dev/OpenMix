#pragma once

#include <QObject>
#include <QPointer>

class QWidget;

namespace OpenMix {

class UpdateDialog;
struct UpdateInfo;

// Cross-platform automatic updates.
//   - Windows/Linux: checks the release feed daily and shows the app-styled
//     UpdateDialog; on Windows it downloads the installer, quits the app, runs
//     a silent install, and relaunches
//   - macOS: Sparkle (via an Objective-C++ bridge), which is native there
//
// initialize() enables the periodic background checks; call it once at startup
// with the main window (dialog parent, closed before installing).
// checkForUpdatesNow() runs a user-triggered check with UI.
class AutoUpdater : public QObject {
    Q_OBJECT

  public:
    explicit AutoUpdater(QObject* parent = nullptr);
    ~AutoUpdater() override;

    void initialize(QWidget* dialogParent);
    void checkForUpdatesNow();

  private:
#if !defined(Q_OS_MACOS)
    void maybeScheduledCheck();
    void runCheck(bool manual);
    void showUpdateDialog(const UpdateInfo& info);
    void launchInstallerAndQuit(const QString& installerPath);

    QWidget* m_dialogParent = nullptr;
    QPointer<UpdateDialog> m_dialog;
#endif
};

} // namespace OpenMix
