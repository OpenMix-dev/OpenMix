#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace OpenMix {

class Application;

// Settings dialog for the show-control surfaces wired up in Application:
//   - MIDI Show Control (MSC) inbound GO/STOP from sysex (MidiInputManager)
//   - inbound OSC remote control (OscRemoteServer)
//   - outbound QLab / DAW remote (QLabClient)
// Drives the live service setters and persists via each service's saveToSettings.
class RemoteControlDialog : public QDialog {
    Q_OBJECT

  public:
    explicit RemoteControlDialog(Application* app, QWidget* parent = nullptr);

    void accept() override;

  private:
    void setupUi();
    void loadValues();
    void applyValues();

    Application* m_app;

    // MIDI Show Control
    QCheckBox* m_mscEnabled = nullptr;
    QSpinBox* m_mscDeviceId = nullptr;

    // inbound OSC
    QCheckBox* m_oscEnabled = nullptr;
    QSpinBox* m_oscPort = nullptr;
    QLabel* m_oscStatus = nullptr;

    // outbound QLab
    QCheckBox* m_qlabEnabled = nullptr;
    QLineEdit* m_qlabHost = nullptr;
    QSpinBox* m_qlabPort = nullptr;
    QSpinBox* m_qlabPreRoll = nullptr;
    QLineEdit* m_qlabWorkspace = nullptr;

    QCheckBox* m_reaperEnabled = nullptr;
    QLineEdit* m_reaperHost = nullptr;
    QSpinBox* m_reaperPort = nullptr;
    QCheckBox* m_reaperRecord = nullptr;
    QSpinBox* m_reaperPreRoll = nullptr;
    QCheckBox* m_reaperAutoDetect = nullptr;
    QSpinBox* m_reaperListenPort = nullptr;

    QCheckBox* m_cuePlayerEnabled = nullptr;
    QLineEdit* m_cuePlayerHost = nullptr;
    QSpinBox* m_cuePlayerPort = nullptr;

    QCheckBox* m_scsEnabled = nullptr;
    QLineEdit* m_scsHost = nullptr;
    QSpinBox* m_scsPort = nullptr;
    QLineEdit* m_scsPassword = nullptr;
};

} // namespace OpenMix
