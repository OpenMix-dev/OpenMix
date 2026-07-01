#include "RemoteControlDialog.h"
#include "app/Application.h"
#include "app/OscRemoteServer.h"
#include "app/CuePlayerClient.h"
#include "app/QLabClient.h"
#include "app/ReaperClient.h"
#include "midi/MidiInputManager.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace OpenMix {

RemoteControlDialog::RemoteControlDialog(Application* app, QWidget* parent)
    : QDialog(parent), m_app(app) {
    setWindowTitle(tr("Remote Control"));
    setupUi();
    loadValues();
}

void RemoteControlDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    // --- MIDI Show Control -------------------------------------------------
    auto* mscBox = new QGroupBox(tr("MIDI Show Control (MSC)"), this);
    auto* mscForm = new QFormLayout(mscBox);
    m_mscEnabled = new QCheckBox(tr("Respond to incoming MSC GO/STOP"), mscBox);
    m_mscDeviceId = new QSpinBox(mscBox);
    m_mscDeviceId->setRange(0, 127);
    m_mscDeviceId->setToolTip(tr("MSC device id to match; 127 responds to all devices"));
    mscForm->addRow(m_mscEnabled);
    mscForm->addRow(tr("Device id:"), m_mscDeviceId);
    auto* mscNote = new QLabel(
        tr("MSC rides on the MIDI input device set in Settings > MIDI Controller."), mscBox);
    mscNote->setWordWrap(true);
    mscForm->addRow(mscNote);
    layout->addWidget(mscBox);

    // --- inbound OSC -------------------------------------------------------
    auto* oscBox = new QGroupBox(tr("Inbound OSC Remote"), this);
    auto* oscForm = new QFormLayout(oscBox);
    m_oscEnabled = new QCheckBox(tr("Listen for OSC control"), oscBox);
    m_oscPort = new QSpinBox(oscBox);
    m_oscPort->setRange(1, 65535);
    m_oscStatus = new QLabel(oscBox);
    oscForm->addRow(m_oscEnabled);
    oscForm->addRow(tr("Listen port:"), m_oscPort);
    oscForm->addRow(tr("Status:"), m_oscStatus);
    auto* oscNote = new QLabel(
        tr("Accepts /go, /stop, /next, /prev, /cue/goto and /ctrl/fadeall."), oscBox);
    oscNote->setWordWrap(true);
    oscForm->addRow(oscNote);
    layout->addWidget(oscBox);

    // --- outbound QLab -----------------------------------------------------
    auto* qlabBox = new QGroupBox(tr("QLab / DAW Remote (outbound)"), this);
    auto* qlabForm = new QFormLayout(qlabBox);
    m_qlabEnabled = new QCheckBox(tr("Fire linked QLab cues on GO"), qlabBox);
    m_qlabHost = new QLineEdit(qlabBox);
    m_qlabHost->setPlaceholderText(tr("127.0.0.1"));
    m_qlabPort = new QSpinBox(qlabBox);
    m_qlabPort->setRange(1, 65535);
    m_qlabPreRoll = new QSpinBox(qlabBox);
    m_qlabPreRoll->setRange(0, 60000);
    m_qlabPreRoll->setSuffix(tr(" ms"));
    m_qlabPreRoll->setSingleStep(50);
    m_qlabWorkspace = new QLineEdit(qlabBox);
    m_qlabWorkspace->setPlaceholderText(tr("(optional workspace id)"));
    qlabForm->addRow(m_qlabEnabled);
    qlabForm->addRow(tr("Host:"), m_qlabHost);
    qlabForm->addRow(tr("Port:"), m_qlabPort);
    qlabForm->addRow(tr("Pre-roll:"), m_qlabPreRoll);
    qlabForm->addRow(tr("Workspace:"), m_qlabWorkspace);
    layout->addWidget(qlabBox);

    // --- REAPER virtual sound check ---------------------------------------
    auto* reaperBox = new QGroupBox(tr("REAPER (Virtual Sound Check)"), this);
    auto* reaperForm = new QFormLayout(reaperBox);
    m_reaperEnabled = new QCheckBox(tr("Link to REAPER"), reaperBox);
    m_reaperHost = new QLineEdit(reaperBox);
    m_reaperHost->setPlaceholderText(tr("REAPER IP address"));
    m_reaperPort = new QSpinBox(reaperBox);
    m_reaperPort->setRange(1, 65535);
    m_reaperRecord = new QCheckBox(tr("Record markers (unchecked jumps to them)"), reaperBox);
    m_reaperPreRoll = new QSpinBox(reaperBox);
    m_reaperPreRoll->setRange(0, 60);
    m_reaperPreRoll->setSuffix(tr(" s"));
    m_reaperAutoDetect =
        new QCheckBox(tr("Auto-detect record/playback from REAPER"), reaperBox);
    m_reaperListenPort = new QSpinBox(reaperBox);
    m_reaperListenPort->setRange(1, 65535);
    reaperForm->addRow(m_reaperEnabled);
    reaperForm->addRow(tr("Host:"), m_reaperHost);
    reaperForm->addRow(tr("Port:"), m_reaperPort);
    reaperForm->addRow(m_reaperRecord);
    reaperForm->addRow(tr("Marker pre-roll:"), m_reaperPreRoll);
    reaperForm->addRow(m_reaperAutoDetect);
    reaperForm->addRow(tr("Listen port:"), m_reaperListenPort);
    layout->addWidget(reaperBox);

    // --- Cue Player (cpsound) ---------------------------------------------
    auto* cpBox = new QGroupBox(tr("Cue Player (cpsound)"), this);
    auto* cpForm = new QFormLayout(cpBox);
    m_cuePlayerEnabled = new QCheckBox(tr("Advance Cue Player on GO"), cpBox);
    m_cuePlayerHost = new QLineEdit(cpBox);
    m_cuePlayerHost->setPlaceholderText(tr("127.0.0.1"));
    m_cuePlayerPort = new QSpinBox(cpBox);
    m_cuePlayerPort->setRange(1, 65535);
    cpForm->addRow(m_cuePlayerEnabled);
    cpForm->addRow(tr("Host:"), m_cuePlayerHost);
    cpForm->addRow(tr("Port:"), m_cuePlayerPort);
    layout->addWidget(cpBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &RemoteControlDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RemoteControlDialog::reject);
    layout->addWidget(buttons);
}

void RemoteControlDialog::loadValues() {
    if (MidiInputManager* midi = m_app->midiInputManager()) {
        m_mscEnabled->setChecked(midi->isMscEnabled());
        m_mscDeviceId->setValue(midi->mscDeviceId());
    }

    if (OscRemoteServer* osc = m_app->oscRemoteServer()) {
        m_oscEnabled->setChecked(osc->isRunning());
        m_oscPort->setValue(osc->port() > 0 ? osc->port() : 8000);
        m_oscStatus->setText(osc->isRunning() ? tr("Listening on port %1").arg(osc->port())
                                              : tr("Stopped"));
    }

    if (QLabClient* qlab = m_app->qLabClient()) {
        m_qlabEnabled->setChecked(qlab->isEnabled());
        m_qlabHost->setText(qlab->host());
        m_qlabPort->setValue(qlab->port());
        m_qlabPreRoll->setValue(qlab->preRollMs());
        m_qlabWorkspace->setText(qlab->workspaceId());
    }

    if (ReaperClient* reaper = m_app->reaperClient()) {
        m_reaperEnabled->setChecked(reaper->isEnabled());
        m_reaperHost->setText(reaper->host());
        m_reaperPort->setValue(reaper->port());
        m_reaperRecord->setChecked(reaper->recordMode());
        m_reaperPreRoll->setValue(reaper->preRollSeconds());
        m_reaperAutoDetect->setChecked(reaper->autoDetect());
        m_reaperListenPort->setValue(reaper->listenPort());
    }

    if (CuePlayerClient* cp = m_app->cuePlayerClient()) {
        m_cuePlayerEnabled->setChecked(cp->isEnabled());
        m_cuePlayerHost->setText(cp->host());
        m_cuePlayerPort->setValue(cp->port());
    }
}

void RemoteControlDialog::applyValues() {
    if (MidiInputManager* midi = m_app->midiInputManager()) {
        midi->setMscEnabled(m_mscEnabled->isChecked());
        midi->setMscDeviceId(m_mscDeviceId->value());
        midi->saveToSettings();
    }

    if (OscRemoteServer* osc = m_app->oscRemoteServer()) {
        // (re)bind to the chosen port when enabled, otherwise release it. The
        // backend persists only the port, so the on/off choice is per-session.
        if (m_oscEnabled->isChecked()) {
            if (!osc->isRunning() || osc->port() != m_oscPort->value()) {
                osc->stop();
                osc->start(m_oscPort->value());
            }
        } else {
            osc->stop();
        }
        osc->saveToSettings();
    }

    if (QLabClient* qlab = m_app->qLabClient()) {
        qlab->setEnabled(m_qlabEnabled->isChecked());
        qlab->setTarget(m_qlabHost->text().trimmed().isEmpty() ? QStringLiteral("127.0.0.1")
                                                               : m_qlabHost->text().trimmed(),
                        m_qlabPort->value());
        qlab->setPreRollMs(m_qlabPreRoll->value());
        qlab->setWorkspaceId(m_qlabWorkspace->text().trimmed());
        qlab->saveToSettings();
    }

    if (ReaperClient* reaper = m_app->reaperClient()) {
        reaper->setEnabled(m_reaperEnabled->isChecked());
        reaper->setTarget(m_reaperHost->text().trimmed().isEmpty()
                              ? QStringLiteral("127.0.0.1")
                              : m_reaperHost->text().trimmed(),
                          m_reaperPort->value());
        reaper->setRecordMode(m_reaperRecord->isChecked());
        reaper->setPreRollSeconds(m_reaperPreRoll->value());
        reaper->setListenPort(m_reaperListenPort->value());
        reaper->setAutoDetect(m_reaperAutoDetect->isChecked());
        reaper->saveToSettings();
    }

    if (CuePlayerClient* cp = m_app->cuePlayerClient()) {
        cp->setEnabled(m_cuePlayerEnabled->isChecked());
        cp->setTarget(m_cuePlayerHost->text().trimmed().isEmpty()
                          ? QStringLiteral("127.0.0.1")
                          : m_cuePlayerHost->text().trimmed(),
                      m_cuePlayerPort->value());
        cp->saveToSettings();
    }
}

void RemoteControlDialog::accept() {
    applyValues();
    QDialog::accept();
}

} // namespace OpenMix
