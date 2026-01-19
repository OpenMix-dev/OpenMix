#include "ShortcutConfigDialog.h"
#include "core/ShortcutManager.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace StageBlend {

ShortcutConfigDialog::ShortcutConfigDialog(ShortcutManager* manager, QWidget* parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle(tr("Configure Shortcuts"));
    setMinimumSize(500, 400);
    setupUi();
    populateTable();
}

void ShortcutConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // instructions
    QLabel* instructions = new QLabel(
        tr("Double-click on a shortcut to edit it. Press Escape to clear a shortcut."), this);
    instructions->setWordWrap(true);
    mainLayout->addWidget(instructions);

    // shortcuts table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Action"), tr("Current Shortcut"), tr("Default")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            &ShortcutConfigDialog::onCellDoubleClicked);
    mainLayout->addWidget(m_table);

    // shortcut editor (hidden by default)
    QHBoxLayout* editorLayout = new QHBoxLayout();
    QLabel* editorLabel = new QLabel(tr("Press new shortcut:"), this);
    m_shortcutEdit = new QKeySequenceEdit(this);
    m_shortcutEdit->setVisible(false);
    connect(m_shortcutEdit, &QKeySequenceEdit::editingFinished, this,
            &ShortcutConfigDialog::onShortcutEdited);
    editorLayout->addWidget(editorLabel);
    editorLayout->addWidget(m_shortcutEdit);
    editorLayout->addStretch();
    mainLayout->addLayout(editorLayout);

    // numpad option
    QGroupBox* numpadGroup = new QGroupBox(tr("Numeric Keypad"), this);
    QVBoxLayout* numpadLayout = new QVBoxLayout(numpadGroup);
    m_numpadCheckbox = new QCheckBox(tr("Enable numeric keypad cue jumping"), this);
    m_numpadCheckbox->setChecked(m_manager->isNumpadCueJumpEnabled());
    m_numpadCheckbox->setToolTip(
        tr("Type cue numbers on the numeric keypad & press Enter to jump to that cue"));
    connect(m_numpadCheckbox, &QCheckBox::stateChanged, this,
            &ShortcutConfigDialog::onNumpadCheckChanged);
    numpadLayout->addWidget(m_numpadCheckbox);
    mainLayout->addWidget(numpadGroup);

    // reset buttons
    QHBoxLayout* resetLayout = new QHBoxLayout();
    m_resetSelectedButton = new QPushButton(tr("Reset Selected"), this);
    m_resetSelectedButton->setEnabled(false);
    connect(m_resetSelectedButton, &QPushButton::clicked, this,
            &ShortcutConfigDialog::onResetSelectedClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            [this]() { m_resetSelectedButton->setEnabled(m_table->currentRow() >= 0); });
    resetLayout->addWidget(m_resetSelectedButton);

    m_resetAllButton = new QPushButton(tr("Reset All to Defaults"), this);
    connect(m_resetAllButton, &QPushButton::clicked, this,
            &ShortcutConfigDialog::onResetAllClicked);
    resetLayout->addWidget(m_resetAllButton);
    resetLayout->addStretch();
    mainLayout->addLayout(resetLayout);

    // dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &ShortcutConfigDialog::accept);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);
}

void ShortcutConfigDialog::populateTable() {
    QList<ShortcutInfo> shortcuts = m_manager->allShortcuts();
    m_table->setRowCount(shortcuts.size());

    int row = 0;
    for (const ShortcutInfo& info : shortcuts) {
        QTableWidgetItem* actionItem = new QTableWidgetItem(info.displayName);
        actionItem->setData(Qt::UserRole, info.id); // store action ID
        m_table->setItem(row, 0, actionItem);

        QTableWidgetItem* currentItem = new QTableWidgetItem(info.currentShortcut.toString());
        m_table->setItem(row, 1, currentItem);

        QTableWidgetItem* defaultItem = new QTableWidgetItem(info.defaultShortcut.toString());
        defaultItem->setForeground(Qt::gray);
        m_table->setItem(row, 2, defaultItem);

        row++;
    }
}

void ShortcutConfigDialog::onResetAllClicked() {
    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("Reset All Shortcuts"),
        tr("Are you sure you want to reset all shortcuts to their default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        m_manager->resetToDefaults();
        populateTable();
    }
}

void ShortcutConfigDialog::onResetSelectedClicked() {
    int row = m_table->currentRow();
    if (row < 0)
        return;

    QString actionId = m_table->item(row, 0)->data(Qt::UserRole).toString();
    m_manager->resetToDefault(actionId);

    // update table
    m_table->item(row, 1)->setText(m_manager->shortcut(actionId).toString());
}

void ShortcutConfigDialog::onCellDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row < 0)
        return;

    m_editingRow = row;
    QString actionId = m_table->item(row, 0)->data(Qt::UserRole).toString();
    m_shortcutEdit->setKeySequence(m_manager->shortcut(actionId));
    m_shortcutEdit->setVisible(true);
    m_shortcutEdit->setFocus();
}

void ShortcutConfigDialog::onShortcutEdited() {
    if (m_editingRow < 0)
        return;

    QString actionId = m_table->item(m_editingRow, 0)->data(Qt::UserRole).toString();
    QKeySequence newShortcut = m_shortcutEdit->keySequence();

    // check for conflicts
    if (m_manager->hasConflict(actionId, newShortcut)) {
        QString conflicting = m_manager->conflictingAction(newShortcut);
        QList<ShortcutInfo> shortcuts = m_manager->allShortcuts();
        QString conflictingName;
        for (const ShortcutInfo& info : shortcuts) {
            if (info.id == conflicting) {
                conflictingName = info.displayName;
                break;
            }
        }

        QMessageBox::warning(this, tr("Shortcut Conflict"),
                             tr("The shortcut '%1' is already assigned to '%2'.\n\n"
                                "Please choose a different shortcut.")
                                 .arg(newShortcut.toString())
                                 .arg(conflictingName));
        return;
    }

    // apply the new shortcut
    m_manager->setShortcut(actionId, newShortcut);
    m_table->item(m_editingRow, 1)->setText(newShortcut.toString());

    m_shortcutEdit->setVisible(false);
    m_editingRow = -1;
}

void ShortcutConfigDialog::onNumpadCheckChanged(int state) {
    m_manager->setNumpadCueJumpEnabled(state == Qt::Checked);
}

void ShortcutConfigDialog::accept() {
    if (!checkConflicts()) {
        return;
    }

    applyChanges();
    QDialog::accept();
}

void ShortcutConfigDialog::applyChanges() { m_manager->saveToSettings(); }

bool ShortcutConfigDialog::checkConflicts() {
    // final conflict check before accepting
    // (Conflicts should already be prevented in onShortcutEdited)
    return true;
}

} // namespace StageBlend
