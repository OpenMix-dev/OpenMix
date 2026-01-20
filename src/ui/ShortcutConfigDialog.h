#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QTableWidget>

namespace OpenMix {

class ShortcutManager;

class ShortcutConfigDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ShortcutConfigDialog(ShortcutManager* manager, QWidget* parent = nullptr);

  private slots:
    void onResetAllClicked();
    void onResetSelectedClicked();
    void onCellDoubleClicked(int row, int column);
    void onShortcutEdited();
    void onNumpadCheckChanged(int state);
    void accept() override;

  private:
    void setupUi();
    void populateTable();
    void applyChanges();
    bool checkConflicts();

    ShortcutManager* m_manager;

    QTableWidget* m_table;
    QKeySequenceEdit* m_shortcutEdit;
    QCheckBox* m_numpadCheckbox;
    QPushButton* m_resetAllButton;
    QPushButton* m_resetSelectedButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    int m_editingRow = -1;
};

} // namespace OpenMix
