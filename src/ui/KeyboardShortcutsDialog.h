#pragma once

#include "core/ShortcutManager.h"

#include <QDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>

namespace OpenMix {

class ShortcutManager;

class KeyboardShortcutsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit KeyboardShortcutsDialog(ShortcutManager* manager, QWidget* parent = nullptr);

  private slots:
    void onItemSelectionChanged();
    void onShortcutEditingFinished();
    void onClearShortcutClicked();
    void onResetSelectedClicked();
    void onResetAllClicked();
    void accept() override;

  private:
    void setupUi();
    void populateShortcutTree();
    void updateShortcutDisplay(QTreeWidgetItem* item, const QKeySequence& shortcut);
    bool checkForConflict(const QString& actionId, const QKeySequence& shortcut);
    QString getCategoryName(const QString& actionId) const;

    ShortcutManager* m_manager;

    // shortcut tree
    QTreeWidget* m_shortcutsTree;
    QLineEdit* m_filterEdit;

    // shortcut editor
    QLabel* m_actionLabel;
    QKeySequenceEdit* m_shortcutEdit;
    QLabel* m_conflictLabel;
    QPushButton* m_clearButton;
    QPushButton* m_resetSelectedButton;

    // dialog buttons
    QPushButton* m_resetAllButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    // pending changes
    QMap<QString, QKeySequence> m_pendingChanges;

    // track selected action
    QString m_currentActionId;
};

} // namespace OpenMix
