#include "EnsemblePanel.h"
#include "app/Application.h"
#include "core/ActorProfileLibrary.h"
#include "core/Ensemble.h"
#include "core/Show.h"
#include "theme/Icons.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <algorithm>

namespace OpenMix {

namespace {
constexpr int EnsembleIdRole = Qt::UserRole + 1;

// parse "1, 2, 5-8" into a sorted, unique channel list
QList<int> parseChannels(const QString& text) {
    QList<int> channels;
    const QStringList tokens = text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        const int dash = token.indexOf('-');
        if (dash > 0) {
            bool okA = false, okB = false;
            int a = token.left(dash).toInt(&okA);
            int b = token.mid(dash + 1).toInt(&okB);
            if (okA && okB && a > 0 && b >= a) {
                for (int c = a; c <= b; ++c)
                    channels.append(c);
            }
        } else {
            bool ok = false;
            int c = token.toInt(&ok);
            if (ok && c > 0)
                channels.append(c);
        }
    }
    std::sort(channels.begin(), channels.end());
    channels.erase(std::unique(channels.begin(), channels.end()), channels.end());
    return channels;
}

QString formatChannels(const QList<int>& channels) {
    QStringList parts;
    for (int c : channels)
        parts.append(QString::number(c));
    return parts.join(", ");
}

QString itemText(const Ensemble& e) {
    const QString name = e.name().isEmpty() ? QObject::tr("(unnamed)") : e.name();
    if (e.channels().isEmpty())
        return name;
    return QString("%1  [%2]").arg(name, formatChannels(e.channels()));
}
} // namespace

EnsemblePanel::EnsemblePanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    if (m_app && m_app->show())
        m_library = m_app->show()->ensembleLibrary();

    setupUi();
    refresh();
}

void EnsemblePanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    m_addButton = new QPushButton(Icons::listAdd(), tr("Add Ensemble"), this);
    m_addButton->setToolTip(tr("Create a new ensemble"));
    connect(m_addButton, &QPushButton::clicked, this, &EnsemblePanel::addEnsemble);
    toolbarLayout->addWidget(m_addButton);

    m_removeButton = new QPushButton(Icons::listRemove(), tr("Remove"), this);
    m_removeButton->setToolTip(tr("Delete the selected ensemble"));
    connect(m_removeButton, &QPushButton::clicked, this, &EnsemblePanel::removeEnsemble);
    toolbarLayout->addWidget(m_removeButton);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // ensemble list
    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &EnsemblePanel::onSelectionChanged);
    mainLayout->addWidget(m_list, 1);

    // editor
    QGroupBox* editorGroup = new QGroupBox(tr("Ensemble"), this);
    QFormLayout* form = new QFormLayout(editorGroup);

    m_nameEdit = new QLineEdit(editorGroup);
    m_nameEdit->setPlaceholderText(tr("Ensemble name"));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &EnsemblePanel::onNameEdited);
    form->addRow(tr("Name:"), m_nameEdit);

    m_channelsEdit = new QLineEdit(editorGroup);
    m_channelsEdit->setPlaceholderText(tr("e.g. 1, 2, 5-8"));
    m_channelsEdit->setToolTip(tr("Comma-separated channels and ranges"));
    connect(m_channelsEdit, &QLineEdit::editingFinished, this, &EnsemblePanel::onChannelsEdited);
    form->addRow(tr("Channels:"), m_channelsEdit);

    m_slotCombo = new QComboBox(editorGroup);
    m_slotCombo->setToolTip(tr("Actor profile slot each member channel recalls"));
    connect(m_slotCombo, &QComboBox::currentTextChanged, this,
            &EnsemblePanel::onProfileSlotChanged);
    form->addRow(tr("Profile slot:"), m_slotCombo);

    m_summaryLabel = new QLabel(editorGroup);
    m_summaryLabel->setWordWrap(true);
    form->addRow(tr("Members:"), m_summaryLabel);

    mainLayout->addWidget(editorGroup);
}

void EnsemblePanel::refresh() {
    if (m_app && m_app->show())
        m_library = m_app->show()->ensembleLibrary();

    // repopulate the profile-slot options from the actor library
    m_updatingUi = true;
    const QString keepSlot = m_slotCombo->currentText();
    m_slotCombo->clear();
    if (m_app && m_app->show() && m_app->show()->actorProfileLibrary())
        m_slotCombo->addItems(m_app->show()->actorProfileLibrary()->profileSlots());
    if (!keepSlot.isEmpty() && m_slotCombo->findText(keepSlot) < 0)
        m_slotCombo->addItem(keepSlot);
    m_updatingUi = false;

    populateList();
    updateEditor();
}

void EnsemblePanel::populateList() {
    const QString keepId = currentEnsembleId();

    m_updatingUi = true;
    m_list->clear();
    if (m_library) {
        for (const Ensemble& e : m_library->ensembles()) {
            QListWidgetItem* item = new QListWidgetItem(itemText(e), m_list);
            item->setData(EnsembleIdRole, e.id());
            if (e.id() == keepId)
                m_list->setCurrentItem(item);
        }
    }
    m_updatingUi = false;
}

QString EnsemblePanel::currentEnsembleId() const {
    QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(EnsembleIdRole).toString() : QString();
}

void EnsemblePanel::setEditorEnabled(bool enabled) {
    m_nameEdit->setEnabled(enabled);
    m_channelsEdit->setEnabled(enabled);
    m_slotCombo->setEnabled(enabled);
    m_removeButton->setEnabled(enabled);
}

void EnsemblePanel::updateEditor() {
    const Ensemble* e = m_library ? m_library->ensembleById(currentEnsembleId()) : nullptr;

    m_updatingUi = true;
    if (e) {
        m_nameEdit->setText(e->name());
        m_channelsEdit->setText(formatChannels(e->channels()));
        int idx = m_slotCombo->findText(e->profileSlot());
        if (idx < 0) {
            m_slotCombo->addItem(e->profileSlot());
            idx = m_slotCombo->findText(e->profileSlot());
        }
        m_slotCombo->setCurrentIndex(idx);
        m_summaryLabel->setText(tr("%n channel(s)", "", e->channels().size()));
        setEditorEnabled(true);
    } else {
        m_nameEdit->clear();
        m_channelsEdit->clear();
        m_summaryLabel->clear();
        setEditorEnabled(false);
    }
    m_updatingUi = false;
}

void EnsemblePanel::addEnsemble() {
    if (!m_library)
        return;
    Ensemble e(tr("New Ensemble"));
    if (m_slotCombo->count() > 0)
        e.setProfileSlot(m_slotCombo->itemText(0));
    m_library->addEnsemble(e);

    QListWidgetItem* item = new QListWidgetItem(itemText(e), m_list);
    item->setData(EnsembleIdRole, e.id());
    m_list->setCurrentItem(item);
    updateEditor();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void EnsemblePanel::removeEnsemble() {
    if (!m_library)
        return;
    const QString id = currentEnsembleId();
    if (id.isEmpty())
        return;
    m_library->removeEnsemble(id);
    delete m_list->takeItem(m_list->currentRow());
    updateEditor();
}

void EnsemblePanel::onSelectionChanged() {
    if (m_updatingUi)
        return;
    updateEditor();
}

void EnsemblePanel::onNameEdited() {
    if (m_updatingUi || !m_library)
        return;
    const Ensemble* current = m_library->ensembleById(currentEnsembleId());
    if (!current || current->name() == m_nameEdit->text())
        return;
    Ensemble e = *current;
    e.setName(m_nameEdit->text());
    m_library->updateEnsemble(e.id(), e);
    if (QListWidgetItem* item = m_list->currentItem())
        item->setText(itemText(e));
}

void EnsemblePanel::onChannelsEdited() {
    if (m_updatingUi || !m_library)
        return;
    const Ensemble* current = m_library->ensembleById(currentEnsembleId());
    if (!current)
        return;
    Ensemble e = *current;
    e.setChannels(parseChannels(m_channelsEdit->text()));
    m_library->updateEnsemble(e.id(), e);

    m_updatingUi = true;
    m_channelsEdit->setText(formatChannels(e.channels())); // reflect normalization
    m_summaryLabel->setText(tr("%n channel(s)", "", e.channels().size()));
    m_updatingUi = false;
    if (QListWidgetItem* item = m_list->currentItem())
        item->setText(itemText(e));
}

void EnsemblePanel::onProfileSlotChanged() {
    if (m_updatingUi || !m_library)
        return;
    const Ensemble* current = m_library->ensembleById(currentEnsembleId());
    if (!current || current->profileSlot() == m_slotCombo->currentText())
        return;
    Ensemble e = *current;
    e.setProfileSlot(m_slotCombo->currentText());
    m_library->updateEnsemble(e.id(), e);
}

} // namespace OpenMix
