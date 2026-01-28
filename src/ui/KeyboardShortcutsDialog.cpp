#include "KeyboardShortcutsDialog.h"
#include "core/ShortcutManager.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QVBoxLayout>

namespace OpenMix {

KeyboardShortcutsDialog::KeyboardShortcutsDialog(ShortcutManager* manager, QWidget* parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle(tr("Keyboard Shortcuts"));
    setMinimumSize(600, 500);

    setupUi();
    populateShortcutTree();
}

void KeyboardShortcutsDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // filter bar
    QHBoxLayout* filterLayout = new QHBoxLayout();
    QLabel* filterLabel = new QLabel(tr("Filter:"), this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Type to filter actions..."));
    m_filterEdit->setClearButtonEnabled(true);

    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (int i = 0; i < m_shortcutsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* category = m_shortcutsTree->topLevelItem(i);
            bool categoryVisible = false;

            for (int j = 0; j < category->childCount(); ++j) {
                QTreeWidgetItem* item = category->child(j);

                bool matches = text.isEmpty() ||
                               item->text(0).contains(text, Qt::CaseInsensitive) ||
                               item->text(1).contains(text, Qt::CaseInsensitive);

                item->setHidden(!matches);

                if (matches) {
                    categoryVisible = true;
                }
            }

            category->setHidden(!categoryVisible);

            if (categoryVisible) {
                category->setExpanded(true);
            }
        }
    });

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_filterEdit);
    mainLayout->addLayout(filterLayout);

    // shortcuts tree
    m_shortcutsTree = new QTreeWidget(this);
    m_shortcutsTree->setHeaderLabels({tr("Action"), tr("Shortcut"), tr("Default")});
    m_shortcutsTree->setColumnCount(3);
    m_shortcutsTree->setRootIsDecorated(true);
    m_shortcutsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_shortcutsTree->setAlternatingRowColors(true);
    m_shortcutsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_shortcutsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(m_shortcutsTree, &QTreeWidget::itemSelectionChanged, this,
            &KeyboardShortcutsDialog::onItemSelectionChanged);

    mainLayout->addWidget(m_shortcutsTree);

    // shortcut editor section
    QGroupBox* editorGroup = new QGroupBox(tr("Edit Shortcut"), this);
    QVBoxLayout* editorLayout = new QVBoxLayout(editorGroup);

    m_actionLabel = new QLabel(tr("Select an action above"), this);
    m_actionLabel->setStyleSheet("font-weight: bold;");
    editorLayout->addWidget(m_actionLabel);

    QHBoxLayout* shortcutEditLayout = new QHBoxLayout();
    m_shortcutEdit = new QKeySequenceEdit(this);
    m_shortcutEdit->setEnabled(false);

    connect(m_shortcutEdit, &QKeySequenceEdit::editingFinished, this,
            &KeyboardShortcutsDialog::onShortcutEditingFinished);

    shortcutEditLayout->addWidget(m_shortcutEdit);

    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setEnabled(false);
    m_clearButton->setToolTip(tr("Remove the shortcut from this action"));

    connect(m_clearButton, &QPushButton::clicked, this,
            &KeyboardShortcutsDialog::onClearShortcutClicked);

    shortcutEditLayout->addWidget(m_clearButton);

    m_resetSelectedButton = new QPushButton(tr("Reset"), this);
    m_resetSelectedButton->setEnabled(false);
    m_resetSelectedButton->setToolTip(tr("Reset to the default shortcut"));
    connect(m_resetSelectedButton, &QPushButton::clicked, this,
            &KeyboardShortcutsDialog::onResetSelectedClicked);

    shortcutEditLayout->addWidget(m_resetSelectedButton);

    editorLayout->addLayout(shortcutEditLayout);

    m_conflictLabel = new QLabel(this);
    m_conflictLabel->setStyleSheet("color: #e74c3c;"); // red for conflict warnings
    m_conflictLabel->setWordWrap(true);
    m_conflictLabel->hide();
    editorLayout->addWidget(m_conflictLabel);

    mainLayout->addWidget(editorGroup);

    // dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_resetAllButton = new QPushButton(tr("Reset All to Defaults"), this);
    connect(m_resetAllButton, &QPushButton::clicked, this,
            &KeyboardShortcutsDialog::onResetAllClicked);

    buttonLayout->addWidget(m_resetAllButton);

    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &KeyboardShortcutsDialog::accept);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void KeyboardShortcutsDialog::populateShortcutTree() {
    m_shortcutsTree->clear();

    QList<ShortcutInfo> shortcuts = m_manager->allShortcuts();

    // organize by category
    QMap<QString, QTreeWidgetItem*> categories;

    for (const ShortcutInfo& info : shortcuts) {
        QString category = getCategoryName(info.id);

        // create category item if nonexistent
        if (!categories.contains(category)) {
            QTreeWidgetItem* categoryItem = new QTreeWidgetItem(m_shortcutsTree);
            categoryItem->setText(0, category);
            categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);

            QFont font = categoryItem->font(0);
            font.setBold(true);

            categoryItem->setFont(0, font);
            categoryItem->setExpanded(true);
            categories[category] = categoryItem;
        }

        // create action item
        QTreeWidgetItem* item = new QTreeWidgetItem(categories[category]);
        item->setText(0, info.displayName);
        item->setData(0, Qt::UserRole, info.id);

        // check for pending changes
        QKeySequence currentShortcut =
            m_pendingChanges.contains(info.id) ? m_pendingChanges[info.id] : info.currentShortcut;

        item->setText(1, currentShortcut.toString(QKeySequence::NativeText));
        item->setText(2, info.defaultShortcut.toString(QKeySequence::NativeText));

        // highlight if different from default
        if (currentShortcut != info.defaultShortcut) {
            item->setForeground(1, QColor("#3498db")); // blue for customized
        }
    }
}

void KeyboardShortcutsDialog::updateShortcutDisplay(QTreeWidgetItem* item,
                                                    const QKeySequence& shortcut) {
    item->setText(1, shortcut.toString(QKeySequence::NativeText));

    QString actionId = item->data(0, Qt::UserRole).toString();
    QKeySequence defaultShortcut = m_manager->defaultShortcut(actionId);

    if (shortcut != defaultShortcut) {
        item->setForeground(1, QColor("#3498db")); // blue for customized
    } else {
        item->setForeground(1, QColor()); // default
    }
}

void KeyboardShortcutsDialog::onItemSelectionChanged() {
    QList<QTreeWidgetItem*> selected = m_shortcutsTree->selectedItems();
    if (selected.isEmpty()) {
        m_currentActionId.clear();
        m_actionLabel->setText(tr("Select an action above"));
        m_shortcutEdit->clear();
        m_shortcutEdit->setEnabled(false);
        m_clearButton->setEnabled(false);
        m_resetSelectedButton->setEnabled(false);
        m_conflictLabel->hide();
        return;
    }

    QTreeWidgetItem* item = selected.first();
    QString actionId = item->data(0, Qt::UserRole).toString();

    if (actionId.isEmpty()) {
        // category item, not action
        m_currentActionId.clear();
        m_actionLabel->setText(tr("Select an action above"));
        m_shortcutEdit->clear();
        m_shortcutEdit->setEnabled(false);
        m_clearButton->setEnabled(false);
        m_resetSelectedButton->setEnabled(false);
        m_conflictLabel->hide();
        return;
    }

    m_currentActionId = actionId;
    m_actionLabel->setText(item->text(0));

    // get current shortcut
    QKeySequence currentShortcut = m_pendingChanges.contains(actionId)
                                       ? m_pendingChanges[actionId]
                                       : m_manager->shortcut(actionId);

    m_shortcutEdit->blockSignals(true);
    m_shortcutEdit->setKeySequence(currentShortcut);
    m_shortcutEdit->blockSignals(false);

    m_shortcutEdit->setEnabled(true);
    m_clearButton->setEnabled(true);
    m_resetSelectedButton->setEnabled(true);
    m_conflictLabel->hide();
}

void KeyboardShortcutsDialog::onShortcutEditingFinished() {
    if (m_currentActionId.isEmpty())
        return;

    QKeySequence newShortcut = m_shortcutEdit->keySequence();

    // check for conflict
    if (checkForConflict(m_currentActionId, newShortcut)) {
        return; // conflict detected
    }

    // store pending change
    m_pendingChanges[m_currentActionId] = newShortcut;

    // update tree display
    QList<QTreeWidgetItem*> selected = m_shortcutsTree->selectedItems();
    if (!selected.isEmpty()) {
        updateShortcutDisplay(selected.first(), newShortcut);
    }

    m_conflictLabel->hide();
}

bool KeyboardShortcutsDialog::checkForConflict(const QString& actionId,
                                               const QKeySequence& shortcut) {
    if (shortcut.isEmpty()) {
        return false;
    }

    // check against other shortcuts
    QList<ShortcutInfo> allShortcuts = m_manager->allShortcuts();

    for (const ShortcutInfo& info : allShortcuts) {
        if (info.id == actionId)
            continue;

        QKeySequence otherShortcut =
            m_pendingChanges.contains(info.id) ? m_pendingChanges[info.id] : info.currentShortcut;

        if (otherShortcut == shortcut) {
            m_conflictLabel->setText(
                tr("Conflict: This shortcut is already assigned to \"%1\".\n"
                   "Please choose a different shortcut or clear the conflicting one first.")
                    .arg(info.displayName));
            m_conflictLabel->show();

            // revert the shortcut edit to the previous value
            QKeySequence currentShortcut = m_pendingChanges.contains(actionId)
                                               ? m_pendingChanges[actionId]
                                               : m_manager->shortcut(actionId);

            m_shortcutEdit->blockSignals(true);
            m_shortcutEdit->setKeySequence(currentShortcut);
            m_shortcutEdit->blockSignals(false);

            return true;
        }
    }

    return false;
}

void KeyboardShortcutsDialog::onClearShortcutClicked() {
    if (m_currentActionId.isEmpty())
        return;

    m_shortcutEdit->clear();
    m_pendingChanges[m_currentActionId] = QKeySequence();

    QList<QTreeWidgetItem*> selected = m_shortcutsTree->selectedItems();
    if (!selected.isEmpty()) {
        updateShortcutDisplay(selected.first(), QKeySequence());
    }

    m_conflictLabel->hide();
}

void KeyboardShortcutsDialog::onResetSelectedClicked() {
    if (m_currentActionId.isEmpty())
        return;

    QKeySequence defaultShortcut = m_manager->defaultShortcut(m_currentActionId);

    // check for conflict with default shortcut
    if (checkForConflict(m_currentActionId, defaultShortcut)) {
        QMessageBox::warning(this, tr("Cannot Reset"),
                             tr("The default shortcut conflicts with another action.\n"
                                "Please clear the conflicting shortcut first."));
        return;
    }

    m_shortcutEdit->setKeySequence(defaultShortcut);
    m_pendingChanges[m_currentActionId] = defaultShortcut;

    QList<QTreeWidgetItem*> selected = m_shortcutsTree->selectedItems();
    if (!selected.isEmpty()) {
        updateShortcutDisplay(selected.first(), defaultShortcut);
    }

    m_conflictLabel->hide();
}

void KeyboardShortcutsDialog::onResetAllClicked() {
    int result = QMessageBox::question(
        this, tr("Reset All Shortcuts"),
        tr("Are you sure you want to reset all keyboard shortcuts to their default values?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    // clear pending changes & set defaults
    m_pendingChanges.clear();

    QList<ShortcutInfo> shortcuts = m_manager->allShortcuts();
    for (const ShortcutInfo& info : shortcuts) {
        m_pendingChanges[info.id] = info.defaultShortcut;
    }

    // refresh the tree
    populateShortcutTree();

    // clear selection and editor
    m_shortcutsTree->clearSelection();
    m_currentActionId.clear();
    m_actionLabel->setText(tr("Select an action above"));
    m_shortcutEdit->clear();
    m_shortcutEdit->setEnabled(false);
    m_clearButton->setEnabled(false);
    m_resetSelectedButton->setEnabled(false);
    m_conflictLabel->hide();
}

QString KeyboardShortcutsDialog::getCategoryName(const QString& actionId) const {
    if (actionId.startsWith("file."))
        return tr("File");
    if (actionId.startsWith("edit."))
        return tr("Edit");
    if (actionId.startsWith("playback."))
        return tr("Playback");
    if (actionId.startsWith("view."))
        return tr("View");
    if (actionId.startsWith("dca."))
        return tr("DCA Mapping");
    if (actionId.startsWith("connection."))
        return tr("Connection");
    if (actionId.startsWith("settings."))
        return tr("Settings");
    if (actionId.startsWith("help."))
        return tr("Help");
    return tr("Other");
}

void KeyboardShortcutsDialog::accept() {
    // apply all pending changes
    for (auto it = m_pendingChanges.begin(); it != m_pendingChanges.end(); ++it) {
        m_manager->setShortcut(it.key(), it.value());
    }

    // save to settings
    m_manager->saveToSettings();

    QDialog::accept();
}

} // namespace OpenMix
