#include "WelcomeDialog.h"
#include "WindowSizing.h"
#include "io/ProjectFile.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace OpenMix {

namespace {
constexpr int PathRole = Qt::UserRole + 1;
}

WelcomeDialog::WelcomeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Welcome to OpenMix"));
    setModal(true);
    resize(560, 460);
    WindowSizing::widenOnShow(this);
    setupUi();
}

void WelcomeDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 20);
    mainLayout->setSpacing(6);

    QLabel* title = new QLabel(tr("OpenMix"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 12);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    QLabel* subtitle = new QLabel(tr("Open a recent show, start a new one, or import a show file."), this);
    subtitle->setEnabled(false); // muted
    mainLayout->addWidget(subtitle);
    mainLayout->addSpacing(16);

    // primary actions
    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* newButton = new QPushButton(tr("New Show"), this);
    newButton->setDefault(true);
    newButton->setMinimumHeight(40);
    connect(newButton, &QPushButton::clicked, this, &WelcomeDialog::chooseNew);
    actions->addWidget(newButton);

    QPushButton* openButton = new QPushButton(tr("Open Show..."), this);
    openButton->setMinimumHeight(40);
    connect(openButton, &QPushButton::clicked, this, &WelcomeDialog::chooseOpen);
    actions->addWidget(openButton);

    QPushButton* importButton = new QPushButton(tr("Import Show..."), this);
    importButton->setMinimumHeight(40);
    importButton->setToolTip(tr("Import a .tmix show file"));
    connect(importButton, &QPushButton::clicked, this, &WelcomeDialog::chooseImport);
    actions->addWidget(importButton);
    mainLayout->addLayout(actions);
    mainLayout->addSpacing(16);

    // recent shows
    QLabel* recentLabel = new QLabel(tr("Recent Shows"), this);
    mainLayout->addWidget(recentLabel);

    m_recentList = new QListWidget(this);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &WelcomeDialog::chooseRecent);
    mainLayout->addWidget(m_recentList, 1);

    const QStringList recent = ProjectFile::recentProjects();
    if (recent.isEmpty()) {
        QListWidgetItem* placeholder = new QListWidgetItem(tr("(No recent shows)"), m_recentList);
        placeholder->setFlags(Qt::NoItemFlags);
    } else {
        for (const QString& path : recent) {
            const QFileInfo info(path);
            QListWidgetItem* item =
                new QListWidgetItem(QString("%1\n%2").arg(info.fileName(), path), m_recentList);
            item->setData(PathRole, path);
            item->setToolTip(path);
        }
    }

    // footer: startup preference + open-recent
    QHBoxLayout* footer = new QHBoxLayout();
    QCheckBox* startupCheck = new QCheckBox(tr("Show this screen at startup"), this);
    startupCheck->setChecked(showAtStartup());
    connect(startupCheck, &QCheckBox::toggled, this, [](bool on) {
        QSettings settings;
        settings.setValue("Welcome/showAtStartup", on);
    });
    footer->addWidget(startupCheck);
    footer->addStretch();

    QPushButton* openRecentButton = new QPushButton(tr("Open Selected"), this);
    connect(openRecentButton, &QPushButton::clicked, this, &WelcomeDialog::chooseRecent);
    footer->addWidget(openRecentButton);
    mainLayout->addLayout(footer);
}

void WelcomeDialog::chooseNew() {
    m_choice = Choice::NewShow;
    accept();
}

void WelcomeDialog::chooseOpen() {
    m_choice = Choice::OpenShow;
    accept();
}

void WelcomeDialog::chooseImport() {
    m_choice = Choice::Import;
    accept();
}

void WelcomeDialog::chooseRecent() {
    QListWidgetItem* item = m_recentList->currentItem();
    if (!item)
        return;
    const QString path = item->data(PathRole).toString();
    if (path.isEmpty())
        return;
    m_choice = Choice::OpenRecent;
    m_recentPath = path;
    accept();
}

bool WelcomeDialog::showAtStartup() {
    QSettings settings;
    return settings.value("Welcome/showAtStartup", true).toBool();
}

} // namespace OpenMix
