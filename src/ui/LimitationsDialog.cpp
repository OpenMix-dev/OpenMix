#include "LimitationsDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace OpenMix {

LimitationsDialog::LimitationsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Important Information"));
    setModal(true);
    setMinimumSize(550, 450);
    setupUi();
}

void LimitationsDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // header w/ icon
    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* iconLabel = new QLabel(this);
    QIcon infoIcon = style()->standardIcon(QStyle::SP_MessageBoxInformation);
    iconLabel->setPixmap(infoIcon.pixmap(48, 48));
    headerLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel(
        tr("<h2>Welcome to OpenMix</h2>"
           "<p>Please read the following important information before using this software.</p>"),
        this);
    titleLabel->setWordWrap(true);
    headerLayout->addWidget(titleLabel, 1);
    mainLayout->addLayout(headerLayout);

    // limitations content
    QTextBrowser* textBrowser = new QTextBrowser(this);
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setHtml(tr(
        "<h3>Current Limitations</h3>"
        "<ul>"
        "<li><b>Mixer Support:</b> Only Behringer X32/M32 mixers are currently supported. "
        "Yamaha, Allen & Heath, & other brands are not yet implemented.</li>"
        "<li><b>Protocol:</b> OSC (Open Sound Control) only. MIDI control is not supported.</li>"
        "<li><b>Parameter Control:</b> OpenMix controls individual parameters only. "
        "Scene/snapshot recall on the mixer is not supported.</li>"
        "<li><b>Network Latency:</b> Fade accuracy depends on network conditions. "
        "Wired Ethernet connections are strongly recommended.</li>"
        "<li><b>Timecode:</b> External timecode synchronization (SMPTE, MTC) is not supported.</li>"
        "</ul>"

        "<h3>Safety Recommendations</h3>"
        "<ul>"
        "<li><b>Always test before performance</b> - Run through your entire cue list in a "
        "non-critical environment before using in a live show.</li>"
        "<li><b>Have a backup plan</b> - Be prepared to operate the mixer manually if needed.</li>"
        "<li><b>Save frequently</b> - Auto-save is enabled, but manual saves are recommended "
        "before major changes.</li>"
        "<li><b>Know the PANIC button</b> - Shift+Escape will fade all parameters to safe "
        "values.</li>"
        "</ul>"

        "<h3>Getting Help</h3>"
        "<p>For documentation, bug reports, & feature requests, visit the project "
        "repository.</p>"));
    mainLayout->addWidget(textBrowser, 1);

    // warning box
    QLabel* warningLabel =
        new QLabel(tr("<p style='color: #856404; background-color: #fff3cd; padding: 10px; "
                      "border-radius: 4px;'>"
                      "<b>Warning:</b> This software is provided as-is without warranty. "
                      "The developers are not responsible for any issues during live performances. "
                      "Always test thoroughly before use in production.</p>"),
                   this);
    warningLabel->setWordWrap(true);
    mainLayout->addWidget(warningLabel);

    // don't show again checkbox
    m_dontShowCheckbox = new QCheckBox(tr("Don't show this message again"), this);
    mainLayout->addWidget(m_dontShowCheckbox);

    // acknowledge button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_acknowledgeButton = new QPushButton(tr("I Understand"), this);
    m_acknowledgeButton->setDefault(true);
    m_acknowledgeButton->setMinimumWidth(120);
    connect(m_acknowledgeButton, &QPushButton::clicked, this,
            &LimitationsDialog::onAcknowledgeClicked);
    buttonLayout->addWidget(m_acknowledgeButton);

    mainLayout->addLayout(buttonLayout);
}

void LimitationsDialog::onAcknowledgeClicked() {
    m_acknowledged = true;

    if (m_dontShowCheckbox->isChecked()) {
        setDontShowAgain(true);
    }

    accept();
}

bool LimitationsDialog::dontShowAgain() const { return m_dontShowCheckbox->isChecked(); }

bool LimitationsDialog::shouldShow() {
    QSettings settings("OpenMix", "OpenMix");
    return !settings.value("LimitationsDialog/dontShowAgain", false).toBool();
}

void LimitationsDialog::setDontShowAgain(bool dontShow) {
    QSettings settings("OpenMix", "OpenMix");
    settings.setValue("LimitationsDialog/dontShowAgain", dontShow);
}

} // namespace OpenMix
