#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QSpinBox;

namespace OpenMix {

class Application;
class SpareBackup;

// Allocate the reserved spare-backup channel to cover an input, then switch to
// the backup now or on the next cue. Drives the show's SpareBackup model.
class AllocateSpareDialog : public QDialog {
    Q_OBJECT

  public:
    explicit AllocateSpareDialog(Application* app, QWidget* parent = nullptr);

  private slots:
    void setSpareChannel();
    void allocate();
    void switchNow();
    void switchLater();
    void removeAllocation();
    void updateStatus();

  private:
    Application* m_app;
    SpareBackup* m_spare;

    QSpinBox* m_spareChannelSpin;
    QSpinBox* m_allocateSpin;
    QLabel* m_statusLabel;
    QPushButton* m_allocateButton;
    QPushButton* m_switchNowButton;
    QPushButton* m_switchLaterButton;
    QPushButton* m_removeButton;
};

} // namespace OpenMix
