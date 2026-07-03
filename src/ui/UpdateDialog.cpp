#include "UpdateDialog.h"
#include "WindowSizing.h"
#include "theme/Theme.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace OpenMix {

namespace {
QString displayVersion(QString v) {
    if (v.startsWith('v'))
        v.remove(0, 1);
    return v;
}

QString megabytes(qint64 bytes) {
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1);
}
} // namespace

UpdateDialog::UpdateDialog(const UpdateInfo& info, QWidget* parent)
    : QDialog(parent), m_info(info), m_net(new QNetworkAccessManager(this)) {
    setWindowTitle(tr("Software Update"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(460);
    WindowSizing::widenOnShow(this);
    setupUi();
    setState(State::Available);
}

UpdateDialog::~UpdateDialog() {
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    if (m_state != State::ReadyToInstall)
        removePartialFile();
}

bool UpdateDialog::installMode() const {
#if defined(Q_OS_WIN)
    return !m_info.installerUrl.isEmpty();
#else
    // no silent self-install off Windows; the env var lets the download path be
    // exercised on a dev box
    return !m_info.installerUrl.isEmpty() &&
           qEnvironmentVariableIsSet("OPENMIX_UPDATE_TEST_INSTALL");
#endif
}

void UpdateDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 20);
    mainLayout->setSpacing(6);

    QLabel* title = new QLabel(tr("A new version of OpenMix is available"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    QLabel* subtitle = new QLabel(tr("OpenMix %1 is now available. You have %2.")
                                      .arg(displayVersion(m_info.version),
                                           QCoreApplication::applicationVersion()),
                                  this);
    subtitle->setEnabled(false); // muted
    mainLayout->addWidget(subtitle);

    QLabel* notesLink = new QLabel(
        QString("<a href=\"%1\">%2</a>").arg(m_info.pageUrl, tr("Release notes")), this);
    notesLink->setOpenExternalLinks(true);
    mainLayout->addWidget(notesLink);
    mainLayout->addSpacing(16);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setTextVisible(false);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addSpacing(10);

    QHBoxLayout* buttons = new QHBoxLayout();
    m_skipButton = new QPushButton(tr("Skip This Version"), this);
    connect(m_skipButton, &QPushButton::clicked, this, [this]() {
        emit skipVersionRequested(m_info.version);
        close();
    });
    buttons->addWidget(m_skipButton);
    buttons->addStretch();

    m_pageButton = new QPushButton(tr("Open Download Page"), this);
    connect(m_pageButton, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl(m_info.pageUrl));
        close();
    });
    buttons->addWidget(m_pageButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &UpdateDialog::cancelDownload);
    buttons->addWidget(m_cancelButton);

    m_laterButton = new QPushButton(tr("Remind Me Later"), this);
    connect(m_laterButton, &QPushButton::clicked, this, &QDialog::close);
    buttons->addWidget(m_laterButton);

    m_installButton = new QPushButton(this);
    m_installButton->setDefault(true);
    m_installButton->setMinimumHeight(32);
    connect(m_installButton, &QPushButton::clicked, this, &UpdateDialog::primaryAction);
    buttons->addWidget(m_installButton);
    mainLayout->addLayout(buttons);
}

void UpdateDialog::setState(State state) {
    m_state = state;

    m_statusLabel->setVisible(state != State::Available);
    m_statusLabel->setStyleSheet(
        state == State::Error ? QString("color: %1;").arg(Theme::Colors::AccentRed) : QString());
    m_progressBar->setVisible(state == State::Downloading);
    m_cancelButton->setVisible(state == State::Downloading);
    m_skipButton->setVisible(state == State::Available);
    m_pageButton->setVisible(state == State::Error);
    m_laterButton->setVisible(state != State::Downloading);
    m_installButton->setVisible(state != State::Downloading);

    switch (state) {
    case State::Available:
        m_installButton->setText(installMode() ? tr("Install and Relaunch")
                                               : tr("Open Download Page"));
        break;
    case State::Downloading:
        m_statusLabel->setText(tr("Downloading OpenMix %1...").arg(displayVersion(m_info.version)));
        break;
    case State::ReadyToInstall:
        m_statusLabel->setText(tr("Download complete. OpenMix will close and restart to install."));
        m_installButton->setText(tr("Install and Relaunch"));
        break;
    case State::Error:
        m_installButton->setText(tr("Retry"));
        m_laterButton->setText(tr("Close"));
        break;
    }
}

void UpdateDialog::primaryAction() {
    switch (m_state) {
    case State::Available:
    case State::Error:
        if (installMode()) {
            startDownload();
        } else {
            QDesktopServices::openUrl(QUrl(m_info.pageUrl));
            close();
        }
        break;
    case State::ReadyToInstall:
        emit readyToInstall(m_downloadPath);
        break;
    case State::Downloading:
        break;
    }
}

void UpdateDialog::startDownload() {
    QString name = QUrl(m_info.installerUrl).fileName();
    if (name.isEmpty())
        name = "OpenMix-update.exe";
    m_downloadPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QDir::separator() + name;

    m_file = new QFile(m_downloadPath, this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showError(tr("Could not write to %1: %2").arg(m_downloadPath, m_file->errorString()));
        return;
    }

    QNetworkRequest req{QUrl(m_info.installerUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "OpenMix");
    m_reply = m_net->get(req);

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_file && m_file->write(m_reply->readAll()) < 0) {
            showError(tr("Could not write to %1: %2").arg(m_downloadPath, m_file->errorString()));
            m_reply->abort();
        }
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0) {
                    m_progressBar->setRange(0, static_cast<int>(total / 1024));
                    m_progressBar->setValue(static_cast<int>(received / 1024));
                    m_statusLabel->setText(tr("Downloading OpenMix %1... %2 MB of %3 MB")
                                               .arg(displayVersion(m_info.version),
                                                    megabytes(received), megabytes(total)));
                } else {
                    m_progressBar->setRange(0, 0); // indeterminate
                    m_statusLabel->setText(tr("Downloading OpenMix %1... %2 MB")
                                               .arg(displayVersion(m_info.version),
                                                    megabytes(received)));
                }
            });
    connect(m_reply, &QNetworkReply::finished, this, &UpdateDialog::finishDownload);

    setState(State::Downloading);
}

void UpdateDialog::cancelDownload() {
    if (m_reply)
        m_reply->abort(); // finishDownload handles cleanup and the state change
}

void UpdateDialog::finishDownload() {
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    if (m_file) {
        m_file->close();
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        removePartialFile();
        // an abort triggered by a write failure has already shown the error
        if (m_state == State::Downloading)
            setState(State::Available);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        removePartialFile();
        showError(tr("Download failed: %1").arg(reply->errorString()));
        return;
    }

    if (!m_info.sha256.isEmpty()) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!m_file->open(QIODevice::ReadOnly) || !hash.addData(m_file)) {
            removePartialFile();
            showError(tr("Could not verify the downloaded installer."));
            return;
        }
        m_file->close();
        if (QString::fromLatin1(hash.result().toHex()) != m_info.sha256) {
            removePartialFile();
            showError(tr("The downloaded installer failed verification. Please try again."));
            return;
        }
    }

    m_file->deleteLater();
    m_file = nullptr;
    // the user already chose to install; the ReadyToInstall state (with its
    // button) is only revisited if the pre-install window close is cancelled
    setState(State::ReadyToInstall);
    emit readyToInstall(m_downloadPath);
}

void UpdateDialog::showError(const QString& message) {
    setState(State::Error);
    m_statusLabel->setText(message);
}

void UpdateDialog::removePartialFile() {
    if (m_file) {
        m_file->close();
        m_file->remove();
        m_file->deleteLater();
        m_file = nullptr;
    }
}

} // namespace OpenMix
