#include "CueFilterBar.h"
#include "CueFilterProxyModel.h"
#include "core/Cue.h"
#include "theme/Icons.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace OpenMix {

CueFilterBar::CueFilterBar(CueFilterProxyModel* proxyModel, QWidget* parent)
    : QWidget(parent), m_proxyModel(proxyModel) {
    setupUi();
}

void CueFilterBar::setupUi() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // search box
    QLabel* searchLabel = new QLabel(tr("Search:"), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter by name, notes..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(200);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CueFilterBar::onSearchTextChanged);

    // type filter
    QLabel* typeLabel = new QLabel(tr("Type:"), this);
    m_typeCombo = new QComboBox(this);
    m_typeCombo->setMinimumWidth(100);
    populateTypeCombo();
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CueFilterBar::onTypeFilterChanged);

    // group filter
    QLabel* groupLabel = new QLabel(tr("Group:"), this);
    m_groupCombo = new QComboBox(this);
    m_groupCombo->setMinimumWidth(100);
    m_groupCombo->addItem(tr("All Groups"), QString());
    connect(m_groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CueFilterBar::onGroupFilterChanged);

    // tag filter
    QLabel* tagLabel = new QLabel(tr("Tag:"), this);
    m_tagCombo = new QComboBox(this);
    m_tagCombo->setMinimumWidth(100);
    m_tagCombo->addItem(tr("All Tags"), QString());
    connect(m_tagCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CueFilterBar::onTagFilterChanged);

    // clear filter action
    m_clearFiltersAction = new QAction(Icons::editClear(), tr("Clear Filters"), this);
    m_clearFiltersAction->setToolTip(tr("Clear all filter settings"));
    connect(m_clearFiltersAction, &QAction::triggered, this, &CueFilterBar::onClearFilters);

    // clear button
    m_clearButton = new QPushButton(Icons::editClear(), tr("Clear Filters"), this);
    m_clearButton->setToolTip(tr("Clear all filter settings"));
    connect(m_clearButton, &QPushButton::clicked, m_clearFiltersAction, &QAction::trigger);

    // layout
    layout->addWidget(searchLabel);
    layout->addWidget(m_searchEdit);
    layout->addSpacing(16);
    layout->addWidget(typeLabel);
    layout->addWidget(m_typeCombo);
    layout->addSpacing(8);
    layout->addWidget(groupLabel);
    layout->addWidget(m_groupCombo);
    layout->addSpacing(8);
    layout->addWidget(tagLabel);
    layout->addWidget(m_tagCombo);
    layout->addStretch();
    layout->addWidget(m_clearButton);
}

void CueFilterBar::populateTypeCombo() {
    m_typeCombo->clear();
    m_typeCombo->addItem(tr("All Types"), -1);
    m_typeCombo->addItem(tr("Snapshot"), static_cast<int>(CueType::Snapshot));
    m_typeCombo->addItem(tr("Stop"), static_cast<int>(CueType::Stop));
    m_typeCombo->addItem(tr("GoTo"), static_cast<int>(CueType::GoTo));
    m_typeCombo->addItem(tr("Wait"), static_cast<int>(CueType::Wait));
    m_typeCombo->addItem(tr("Macro"), static_cast<int>(CueType::Macro));
}

void CueFilterBar::updateFilterOptions() {
    // save current selections
    QString currentGroup = m_groupCombo->currentData().toString();
    QString currentTag = m_tagCombo->currentData().toString();

    // update group combo
    m_groupCombo->blockSignals(true);
    m_groupCombo->clear();
    m_groupCombo->addItem(tr("All Groups"), QString());
    QStringList groups = m_proxyModel->availableGroups();
    for (const QString& group : groups) {
        m_groupCombo->addItem(group, group);
    }
    // restore selection
    int groupIdx = m_groupCombo->findData(currentGroup);
    if (groupIdx >= 0)
        m_groupCombo->setCurrentIndex(groupIdx);
    m_groupCombo->blockSignals(false);

    // update tag combo
    m_tagCombo->blockSignals(true);
    m_tagCombo->clear();
    m_tagCombo->addItem(tr("All Tags"), QString());
    QStringList tags = m_proxyModel->availableTags();
    for (const QString& tag : tags) {
        m_tagCombo->addItem(tag, tag);
    }
    // restore selection
    int tagIdx = m_tagCombo->findData(currentTag);
    if (tagIdx >= 0)
        m_tagCombo->setCurrentIndex(tagIdx);
    m_tagCombo->blockSignals(false);
}

void CueFilterBar::onSearchTextChanged(const QString& text) {
    if (text.isEmpty()) {
        m_proxyModel->clearTextFilter();
    } else {
        m_proxyModel->setTextFilter(text);
    }
    emit filtersChanged();
}

void CueFilterBar::onTypeFilterChanged(int index) {
    int typeValue = m_typeCombo->itemData(index).toInt();
    if (typeValue < 0) {
        m_proxyModel->clearTypeFilter();
    } else {
        m_proxyModel->setTypeFilter(static_cast<CueType>(typeValue));
    }
    emit filtersChanged();
}

void CueFilterBar::onGroupFilterChanged(int index) {
    QString group = m_groupCombo->itemData(index).toString();
    if (group.isEmpty()) {
        m_proxyModel->clearGroupFilter();
    } else {
        m_proxyModel->setGroupFilter(group);
    }
    emit filtersChanged();
}

void CueFilterBar::onTagFilterChanged(int index) {
    QString tag = m_tagCombo->itemData(index).toString();
    if (tag.isEmpty()) {
        m_proxyModel->clearTagFilter();
    } else {
        m_proxyModel->setTagFilter(tag);
    }
    emit filtersChanged();
}

void CueFilterBar::onClearFilters() {
    m_searchEdit->clear();
    m_typeCombo->setCurrentIndex(0);
    m_groupCombo->setCurrentIndex(0);
    m_tagCombo->setCurrentIndex(0);
    m_proxyModel->clearAllFilters();
    emit filtersChanged();
}

} // namespace OpenMix
