#include "RecoveryDialog.h"
#include "io/CrashRecovery.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLocale>
#include <QStyle>
#include <QVBoxLayout>

namespace StageBlend {

RecoveryDialog::RecoveryDialog(const CrashState& state, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Session Recovery"));
    setModal(true);
    setMinimumWidth(450);
    setupUi(state);
}

void RecoveryDialog::setupUi(const CrashState& state) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // top section with icon & message
    QHBoxLayout* topLayout = new QHBoxLayout();

    m_iconLabel = new QLabel(this);
    QIcon warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    m_iconLabel->setPixmap(warningIcon.pixmap(48, 48));
    topLayout->addWidget(m_iconLabel);

    QVBoxLayout* messageLayout = new QVBoxLayout();
    m_messageLabel = new QLabel(tr("<b>StageBlend did not exit cleanly</b>"), this);
    messageLayout->addWidget(m_messageLabel);

    m_detailsLabel = new QLabel(this);
    m_detailsLabel->setWordWrap(true);
    m_detailsLabel->setStyleSheet("color: gray;");

    // build details text
    QStringList details;
    if (!state.projectPath.isEmpty()) {
        QFileInfo fileInfo(state.projectPath);
        details.append(tr("Project: %1").arg(fileInfo.fileName()));
    } else {
        details.append(tr("Project: Unsaved show"));
    }

    if (state.timestamp > 0) {
        QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(state.timestamp);
        details.append(
            tr("Last saved: %1").arg(QLocale::system().toString(timestamp, QLocale::ShortFormat)));

        // calculate time since crash
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 ageSeconds = (now - state.timestamp) / 1000;
        if (ageSeconds < 60) {
            details.append(tr("(%1 seconds ago)").arg(ageSeconds));
        } else if (ageSeconds < 3600) {
            details.append(tr("(%1 minutes ago)").arg(ageSeconds / 60));
        } else {
            details.append(tr("(%1 hours ago)").arg(ageSeconds / 3600));
        }
    }

    if (state.standbyCueIndex >= 0) {
        details.append(tr("Standby cue index: %1").arg(state.standbyCueIndex));
    }

    m_detailsLabel->setText(details.join("\n"));
    messageLayout->addWidget(m_detailsLabel);

    topLayout->addLayout(messageLayout, 1);
    mainLayout->addLayout(topLayout);

    // separator line
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // description
    QLabel* descLabel = new QLabel(
        tr("Your previous session can be recovered. Choose how you want to proceed:"), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // buttons
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(8);

    m_recoverButton = new QPushButton(tr("Recover Previous Session"), this);
    m_recoverButton->setDefault(true);
    m_recoverButton->setMinimumHeight(36);
    m_recoverButton->setToolTip(tr("Restore your show to the state before the crash"));
    connect(m_recoverButton, &QPushButton::clicked, this, &RecoveryDialog::onRecoverClicked);
    buttonLayout->addWidget(m_recoverButton);

    m_startFreshButton = new QPushButton(tr("Start Fresh"), this);
    m_startFreshButton->setMinimumHeight(36);
    m_startFreshButton->setToolTip(tr("Discard recovery data & start a new show"));
    connect(m_startFreshButton, &QPushButton::clicked, this, &RecoveryDialog::onStartFreshClicked);
    buttonLayout->addWidget(m_startFreshButton);

    m_openBackupButton = new QPushButton(tr("Open Last Saved File..."), this);
    m_openBackupButton->setMinimumHeight(36);
    m_openBackupButton->setToolTip(tr("Browse for a backup or previously saved file"));
    m_openBackupButton->setEnabled(!state.projectPath.isEmpty());
    connect(m_openBackupButton, &QPushButton::clicked, this, &RecoveryDialog::onOpenBackupClicked);
    buttonLayout->addWidget(m_openBackupButton);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
}

void RecoveryDialog::onRecoverClicked() {
    m_selectedAction = Recover;
    accept();
}

void RecoveryDialog::onStartFreshClicked() {
    m_selectedAction = StartFresh;
    accept();
}

void RecoveryDialog::onOpenBackupClicked() {
    m_selectedAction = OpenBackup;
    accept();
}

} // namespace StageBlend
