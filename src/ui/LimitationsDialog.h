#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QPushButton>

namespace OpenMix {

class LimitationsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit LimitationsDialog(QWidget* parent = nullptr);

    // check if user has acknowledged limitations
    bool userAcknowledged() const { return m_acknowledged; }

    // check if user wants to see this dialog again
    bool dontShowAgain() const;

    // static helper to check if dialog should be shown
    static bool shouldShow();
    static void setDontShowAgain(bool dontShow);

  private slots:
    void onAcknowledgeClicked();

  private:
    void setupUi();

    bool m_acknowledged = false;
    QCheckBox* m_dontShowCheckbox;
    QPushButton* m_acknowledgeButton;
};

} // namespace OpenMix
