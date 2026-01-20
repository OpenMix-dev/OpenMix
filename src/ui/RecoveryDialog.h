#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

namespace OpenMix {

struct CrashState;

class RecoveryDialog : public QDialog {
    Q_OBJECT

  public:
    enum Result { Recover, StartFresh, OpenBackup };

    explicit RecoveryDialog(const CrashState& state, QWidget* parent = nullptr);

    Result selectedAction() const { return m_selectedAction; }

  private slots:
    void onRecoverClicked();
    void onStartFreshClicked();
    void onOpenBackupClicked();

  private:
    void setupUi(const CrashState& state);

    Result m_selectedAction = StartFresh;

    QLabel* m_iconLabel;
    QLabel* m_messageLabel;
    QLabel* m_detailsLabel;
    QPushButton* m_recoverButton;
    QPushButton* m_startFreshButton;
    QPushButton* m_openBackupButton;
};

} // namespace OpenMix
