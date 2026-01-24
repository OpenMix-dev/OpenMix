#include "MacroPreviewWidget.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "theme/Theme.h"

#include <QHeaderView>
#include <QSet>

namespace OpenMix {

MacroPreviewWidget::MacroPreviewWidget(QWidget* parent) : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);

    // header label
    m_headerLabel = new QLabel(this);
    m_headerLabel->setProperty("role", "header");
    m_layout->addWidget(m_headerLabel);

    // tree widget for child cues
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels({tr("Cue"), tr("Name"), tr("Type")});
    m_treeWidget->setColumnCount(3);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->header()->setStretchLastSection(true);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &MacroPreviewWidget::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this,
            &MacroPreviewWidget::onItemDoubleClicked);

    m_layout->addWidget(m_treeWidget);

    // empty state label
    m_emptyLabel = new QLabel(tr("Select a macro cue to preview its children"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setProperty("role", "muted");
    m_layout->addWidget(m_emptyLabel);

    clear();
}

void MacroPreviewWidget::setMacroCue(const Cue* cue, const CueList* cueList) {
    m_cue = cue;
    m_cueList = cueList;

    m_treeWidget->clear();

    if (!cue || cue->type() != CueType::Macro) {
        clear();
        return;
    }

    // show the tree widget
    m_treeWidget->setVisible(true);
    m_emptyLabel->setVisible(false);

    // update header
    QString header = tr("Macro: %1 - %2")
                         .arg(cue->number(), 0, 'f', 1)
                         .arg(cue->name().isEmpty() ? tr("(unnamed)") : cue->name());
    header += QString(" (%1)").arg(executionModeString(cue));
    m_headerLabel->setText(header);

    // build the tree
    QSet<QString> visited;
    for (const QString& childId : cue->childCueIds()) {
        const Cue* childCue = cueList->findById(childId);
        if (childCue) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_treeWidget);
            item->setText(0, QString::number(childCue->number(), 'f', 1));
            item->setText(1, childCue->name().isEmpty() ? tr("(unnamed)") : childCue->name());
            item->setText(2, cueTypeToString(childCue->type()));
            item->setData(0, Qt::UserRole, childId);

            // set icon based on cue type
            item->setText(0, cueTypeIcon(childCue) + " " + item->text(0));

            // if this child is also a macro, recursively add its children
            if (childCue->type() == CueType::Macro) {
                buildTree(item, childCue, cueList, 1, visited);
            }
        } else {
            // missing child cue
            QTreeWidgetItem* item = new QTreeWidgetItem(m_treeWidget);
            item->setText(0, "?");
            item->setText(1, tr("[Missing: %1]").arg(childId));
            item->setText(2, "-");
            QColor errorColor = Theme::color(Theme::Colors::AccentRed);
            item->setForeground(0, errorColor);
            item->setForeground(1, errorColor);
            item->setData(0, Qt::UserRole, childId);
        }
    }

    m_treeWidget->expandAll();
}

void MacroPreviewWidget::clear() {
    m_cue = nullptr;
    m_cueList = nullptr;
    m_treeWidget->clear();
    m_treeWidget->setVisible(false);
    m_emptyLabel->setVisible(true);
    m_headerLabel->setText(tr("Macro Preview"));
}

void MacroPreviewWidget::onItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (item) {
        QString cueId = item->data(0, Qt::UserRole).toString();
        if (!cueId.isEmpty()) {
            emit childCueClicked(cueId);
        }
    }
}

void MacroPreviewWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (item) {
        QString cueId = item->data(0, Qt::UserRole).toString();
        if (!cueId.isEmpty()) {
            emit childCueDoubleClicked(cueId);
        }
    }
}

void MacroPreviewWidget::buildTree(QTreeWidgetItem* parent, const Cue* cue, const CueList* cueList,
                                   int depth, QSet<QString>& visited) {
    if (!cue || !cueList || depth > 10) { // limit depth to prevent infinite recursion
        return;
    }

    if (visited.contains(cue->id())) {
        // circular reference detected
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, "!");
        item->setText(1, tr("[Circular reference]"));
        item->setText(2, "-");
        QColor errorColor = Theme::color(Theme::Colors::AccentRed);
        item->setForeground(0, errorColor);
        item->setForeground(1, errorColor);
        return;
    }

    visited.insert(cue->id());

    for (const QString& childId : cue->childCueIds()) {
        const Cue* childCue = cueList->findById(childId);
        if (childCue) {
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, cueTypeIcon(childCue) + " " +
                                 QString::number(childCue->number(), 'f', 1));
            item->setText(1, childCue->name().isEmpty() ? tr("(unnamed)") : childCue->name());
            item->setText(2, cueTypeToString(childCue->type()));
            item->setData(0, Qt::UserRole, childId);

            if (childCue->type() == CueType::Macro) {
                buildTree(item, childCue, cueList, depth + 1, visited);
            }
        } else {
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, "?");
            item->setText(1, tr("[Missing: %1]").arg(childId));
            item->setText(2, "-");
            QColor errorColor = Theme::color(Theme::Colors::AccentRed);
            item->setForeground(0, errorColor);
            item->setForeground(1, errorColor);
            item->setData(0, Qt::UserRole, childId);
        }
    }

    visited.remove(cue->id());
}

QString MacroPreviewWidget::cueTypeIcon(const Cue* cue) const {
    if (!cue)
        return QString();

    switch (cue->type()) {
    case CueType::Snapshot:
        return QString::fromUtf8("\u2B24"); // filled circle
    case CueType::Stop:
        return QString::fromUtf8("\u25A0"); // filled square
    case CueType::GoTo:
        return QString::fromUtf8("\u21B5"); // return arrow
    case CueType::Wait:
        return QString::fromUtf8("\u23F1"); // timer
    case CueType::Macro:
        return QString::fromUtf8("\u2637"); // trigram
    }
    return QString();
}

QString MacroPreviewWidget::executionModeString(const Cue* cue) const {
    if (!cue || cue->type() != CueType::Macro) {
        return QString();
    }

    switch (cue->macroExecutionMode()) {
    case MacroExecutionMode::Sequential:
        return tr("Sequential");
    case MacroExecutionMode::Parallel:
        return tr("Parallel");
    }
    return QString();
}

} // namespace OpenMix
