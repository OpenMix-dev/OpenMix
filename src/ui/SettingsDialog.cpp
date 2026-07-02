#include "SettingsDialog.h"
#include "app/Application.h"
#include "app/QLabClient.h"
#include "core/ChannelMonitor.h"
#include "core/PlaybackEngine.h"
#include "core/ScribbleController.h"
#include "core/Show.h"
#include "theme/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace OpenMix {

SettingsDialog::SettingsDialog(Application* app, QWidget* parent) : QDialog(parent), m_app(app) {
    setWindowTitle(tr("Settings"));
    setupUi();
    loadValues();
}

void SettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    // --- console behavior --------------------------------------------------
    auto* consoleBox = new QGroupBox(tr("Console Behavior"), this);
    auto* consoleForm = new QFormLayout(consoleBox);
    m_dimDcaFaders = new QCheckBox(tr("Dim a DCA's fader when a cue mutes it"), consoleBox);
    m_muteDcaUnassign = new QCheckBox(tr("Unassign a DCA's channels when a cue mutes it"),
                                      consoleBox);
    m_selectOnSpill = new QCheckBox(tr("Select channel on spill"), consoleBox);
    m_suppressBackupSwitch = new QCheckBox(tr("Suppress backup-switch prompt"), consoleBox);
    consoleForm->addRow(m_dimDcaFaders);
    consoleForm->addRow(m_muteDcaUnassign);
    consoleForm->addRow(m_selectOnSpill);
    consoleForm->addRow(m_suppressBackupSwitch);
    layout->addWidget(consoleBox);

    // --- show metadata -----------------------------------------------------
    auto* metaBox = new QGroupBox(tr("Show Metadata"), this);
    auto* metaForm = new QFormLayout(metaBox);
    m_designer = new QLineEdit(metaBox);
    m_designer->setPlaceholderText(tr("Sound designer"));
    metaForm->addRow(tr("Designer:"), m_designer);
    layout->addWidget(metaBox);

    // --- scribble highlight ------------------------------------------------
    auto* scribbleBox = new QGroupBox(tr("Active-Channel Highlight"), this);
    auto* scribbleForm = new QFormLayout(scribbleBox);
    m_highlightEnabled = new QCheckBox(tr("Highlight the channels the current cue touches"),
                                       scribbleBox);
    m_highlightColor = new QSpinBox(scribbleBox);
    m_highlightColor->setRange(0, 15);
    m_highlightColor->setToolTip(tr("Driver-mapped scribble-strip palette index"));
    scribbleForm->addRow(m_highlightEnabled);
    scribbleForm->addRow(tr("Highlight color:"), m_highlightColor);
    layout->addWidget(scribbleBox);

    // --- channel monitor ---------------------------------------------------
    auto* monitorBox = new QGroupBox(tr("Channel Monitor"), this);
    auto* monitorForm = new QFormLayout(monitorBox);
    m_silenceThreshold = new QDoubleSpinBox(monitorBox);
    m_silenceThreshold->setRange(0.0, 1.0);
    m_silenceThreshold->setSingleStep(0.01);
    m_silenceThreshold->setDecimals(3);
    m_clipThreshold = new QDoubleSpinBox(monitorBox);
    m_clipThreshold->setRange(0.0, 1.0);
    m_clipThreshold->setSingleStep(0.01);
    m_clipThreshold->setDecimals(3);
    m_silenceTimeoutMs = new QSpinBox(monitorBox);
    m_silenceTimeoutMs->setRange(0, 60000);
    m_silenceTimeoutMs->setSingleStep(500);
    m_silenceTimeoutMs->setSuffix(tr(" ms"));
    m_clipHoldMs = new QSpinBox(monitorBox);
    m_clipHoldMs->setRange(0, 60000);
    m_clipHoldMs->setSingleStep(500);
    m_clipHoldMs->setSuffix(tr(" ms"));
    monitorForm->addRow(tr("Silence floor:"), m_silenceThreshold);
    monitorForm->addRow(tr("Clip ceiling:"), m_clipThreshold);
    monitorForm->addRow(tr("Silence timeout:"), m_silenceTimeoutMs);
    monitorForm->addRow(tr("Clip hold:"), m_clipHoldMs);
    layout->addWidget(monitorBox);

    // --- QLab remote -------------------------------------------------------
    auto* qlabBox = new QGroupBox(tr("QLab Remote"), this);
    auto* qlabForm = new QFormLayout(qlabBox);
    m_qlabPasscode = new QLineEdit(qlabBox);
    m_qlabPasscode->setEchoMode(QLineEdit::Password);
    m_qlabPasscode->setPlaceholderText(tr("(workspace passcode)"));
    m_qlabSuppressBack = new QCheckBox(tr("Never rewind the QLab playhead"), qlabBox);
    qlabForm->addRow(tr("Passcode:"), m_qlabPasscode);
    qlabForm->addRow(m_qlabSuppressBack);
    layout->addWidget(qlabBox);

    // --- display -----------------------------------------------------------
    auto* displayBox = new QGroupBox(tr("Display"), this);
    auto* displayForm = new QFormLayout(displayBox);
    m_highContrast = new QCheckBox(tr("High-contrast mode (brighter text and borders for the booth)"),
                                   displayBox);
    displayForm->addRow(m_highContrast);
    layout->addWidget(displayBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::loadValues() {
    if (Show* show = m_app->show()) {
        m_dimDcaFaders->setChecked(show->dimDcaFaders());
        m_selectOnSpill->setChecked(show->selectOnSpill());
        m_muteDcaUnassign->setChecked(show->muteDcaUnassign());
        m_suppressBackupSwitch->setChecked(show->suppressBackupSwitch());
        m_designer->setText(show->designer());
    }

    if (ScribbleController* scribble = m_app->scribbleController()) {
        m_highlightEnabled->setChecked(scribble->isHighlightEnabled());
        m_highlightColor->setValue(scribble->highlightColor());
    }

    if (ChannelMonitor* monitor = m_app->channelMonitor()) {
        m_silenceThreshold->setValue(monitor->silenceThreshold());
        m_clipThreshold->setValue(monitor->clipThreshold());
        m_silenceTimeoutMs->setValue(monitor->silenceTimeoutMs());
        m_clipHoldMs->setValue(monitor->clipHoldMs());
    }

    if (QLabClient* qlab = m_app->qLabClient()) {
        m_qlabPasscode->setText(qlab->passcode());
        m_qlabSuppressBack->setChecked(qlab->suppressBack());
    }

    m_highContrast->setChecked(QSettings().value("highContrast", false).toBool());
}

void SettingsDialog::applyValues() {
    if (Show* show = m_app->show()) {
        show->setDimDcaFaders(m_dimDcaFaders->isChecked());
        show->setSelectOnSpill(m_selectOnSpill->isChecked());
        show->setMuteDcaUnassign(m_muteDcaUnassign->isChecked());
        show->setSuppressBackupSwitch(m_suppressBackupSwitch->isChecked());
        show->setDesigner(m_designer->text());
    }

    // push the DCA-apply toggles to the live engine
    if (PlaybackEngine* engine = m_app->playbackEngine()) {
        engine->setDimDcaFaders(m_dimDcaFaders->isChecked());
        engine->setMuteDcaUnassign(m_muteDcaUnassign->isChecked());
    }

    // scribble highlight: drive the live controller and persist via QSettings
    if (ScribbleController* scribble = m_app->scribbleController()) {
        scribble->setHighlightColor(m_highlightColor->value());
        scribble->setHighlightEnabled(m_highlightEnabled->isChecked());
    }
    {
        QSettings settings;
        settings.beginGroup("Scribble");
        settings.setValue("highlightEnabled", m_highlightEnabled->isChecked());
        settings.setValue("highlightColor", m_highlightColor->value());
        settings.endGroup();
    }

    // channel monitor thresholds: drive the live monitor and persist via QSettings
    if (ChannelMonitor* monitor = m_app->channelMonitor()) {
        monitor->setSilenceThreshold(m_silenceThreshold->value());
        monitor->setClipThreshold(m_clipThreshold->value());
        monitor->setSilenceTimeoutMs(m_silenceTimeoutMs->value());
        monitor->setClipHoldMs(m_clipHoldMs->value());
    }
    {
        QSettings settings;
        settings.beginGroup("Monitor");
        settings.setValue("silenceThreshold", m_silenceThreshold->value());
        settings.setValue("clipThreshold", m_clipThreshold->value());
        settings.setValue("silenceTimeoutMs", m_silenceTimeoutMs->value());
        settings.setValue("clipHoldMs", m_clipHoldMs->value());
        settings.endGroup();
    }

    if (QLabClient* qlab = m_app->qLabClient()) {
        qlab->setPasscode(m_qlabPasscode->text());
        qlab->setSuppressBack(m_qlabSuppressBack->isChecked());
        qlab->saveToSettings();
    }

    // high-contrast theme: persist and re-skin the running app immediately
    {
        const bool hc = m_highContrast->isChecked();
        QSettings().setValue("highContrast", hc);
        if (auto* app = qobject_cast<QApplication*>(QApplication::instance()))
            app->setStyleSheet(Theme::globalStylesheet(hc));
    }
}

void SettingsDialog::accept() {
    applyValues();
    QDialog::accept();
}

} // namespace OpenMix
