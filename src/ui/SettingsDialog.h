#pragma once

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

namespace OpenMix {

class Application;

// Application settings dialog for the console-workflow refinements:
//   - console-behavior toggles (Show + PlaybackEngine)
//   - show metadata (designer)
//   - active-channel scribble highlight (ScribbleController)
//   - channel monitor silence/clip thresholds (ChannelMonitor)
//   - QLab passcode / suppress-back (QLabClient)
// Drives the live objects and persists (project fields on the Show, the rest via
// QSettings / each service's saveToSettings).
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit SettingsDialog(Application* app, QWidget* parent = nullptr);

    void accept() override;

  private:
    void setupUi();
    void loadValues();
    void applyValues();

    Application* m_app;

    // console behavior
    QCheckBox* m_dimDcaFaders = nullptr;
    QCheckBox* m_selectOnSpill = nullptr;
    QCheckBox* m_muteDcaUnassign = nullptr;
    QCheckBox* m_suppressBackupSwitch = nullptr;

    // show metadata
    QLineEdit* m_designer = nullptr;

    // scribble highlight
    QCheckBox* m_highlightEnabled = nullptr;
    QSpinBox* m_highlightColor = nullptr;

    // channel monitor thresholds
    QDoubleSpinBox* m_silenceThreshold = nullptr;
    QDoubleSpinBox* m_clipThreshold = nullptr;
    QSpinBox* m_silenceTimeoutMs = nullptr;
    QSpinBox* m_clipHoldMs = nullptr;

    // QLab remote
    QLineEdit* m_qlabPasscode = nullptr;
    QCheckBox* m_qlabSuppressBack = nullptr;

    QCheckBox* m_highContrast = nullptr;
};

} // namespace OpenMix
