#pragma once

#include "app/UpdateChecker.h"

#include <QDialog>

class QFile;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;

namespace OpenMix {

// App-styled update prompt (replaces the stock WinSparkle window). Non-modal so
// a scheduled check can never block a live show. Two modes:
//   - install: downloads the installer with in-dialog progress, verifies its
//     sha256, then signals readyToInstall so AutoUpdater can relaunch
//   - link: no installer published for this platform; opens the download page
// The dialog deletes itself on close.
class UpdateDialog : public QDialog {
    Q_OBJECT

  public:
    explicit UpdateDialog(const UpdateInfo& info, QWidget* parent = nullptr);
    ~UpdateDialog() override;

  signals:
    void skipVersionRequested(const QString& version);
    void readyToInstall(const QString& installerPath);

  private:
    enum class State { Available, Downloading, ReadyToInstall, Error };

    [[nodiscard]] bool installMode() const;
    void setupUi();
    void setState(State state);
    void primaryAction();
    void startDownload();
    void cancelDownload();
    void finishDownload();
    void showError(const QString& message);
    void removePartialFile();

    UpdateInfo m_info;
    State m_state = State::Available;
    QString m_downloadPath;

    QNetworkAccessManager* m_net;
    QNetworkReply* m_reply = nullptr;
    QFile* m_file = nullptr;

    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QPushButton* m_installButton;
    QPushButton* m_laterButton;
    QPushButton* m_skipButton;
    QPushButton* m_cancelButton;
    QPushButton* m_pageButton;
};

} // namespace OpenMix
