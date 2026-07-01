#include "PositionPanel.h"
#include "app/Application.h"
#include "core/Position.h"
#include "core/Show.h"
#include "theme/Icons.h"

#include <QDoubleSpinBox>
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
constexpr int PositionIdRole = Qt::UserRole + 1;

QList<int> parseBuses(const QString& text) {
    QList<int> buses;
    const QStringList tokens = text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        bool ok = false;
        int b = token.toInt(&ok);
        if (ok && b > 0)
            buses.append(b);
    }
    std::sort(buses.begin(), buses.end());
    buses.erase(std::unique(buses.begin(), buses.end()), buses.end());
    return buses;
}

QString formatBuses(const QList<int>& buses) {
    QStringList parts;
    for (int b : buses)
        parts.append(QString::number(b));
    return parts.join(", ");
}

QString itemText(const Position& p) {
    const QString name = p.name().isEmpty() ? QObject::tr("(unnamed)") : p.name();
    if (p.shortName().isEmpty())
        return name;
    return QString("%1  [%2]").arg(name, p.shortName());
}
} // namespace

PositionPanel::PositionPanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    if (m_app && m_app->show())
        m_library = m_app->show()->positionLibrary();

    setupUi();
    refresh();
}

void PositionPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    m_addButton = new QPushButton(Icons::listAdd(), tr("Add Position"), this);
    m_addButton->setToolTip(tr("Create a new position"));
    connect(m_addButton, &QPushButton::clicked, this, &PositionPanel::addPosition);
    toolbarLayout->addWidget(m_addButton);

    m_removeButton = new QPushButton(Icons::listRemove(), tr("Remove"), this);
    m_removeButton->setToolTip(tr("Delete the selected position"));
    connect(m_removeButton, &QPushButton::clicked, this, &PositionPanel::removePosition);
    toolbarLayout->addWidget(m_removeButton);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &PositionPanel::onSelectionChanged);
    mainLayout->addWidget(m_list, 1);

    QGroupBox* editorGroup = new QGroupBox(tr("Position"), this);
    QFormLayout* form = new QFormLayout(editorGroup);

    m_nameEdit = new QLineEdit(editorGroup);
    m_nameEdit->setPlaceholderText(tr("Position name"));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &PositionPanel::onFieldsEdited);
    form->addRow(tr("Name:"), m_nameEdit);

    m_shortNameEdit = new QLineEdit(editorGroup);
    m_shortNameEdit->setPlaceholderText(tr("Short label"));
    connect(m_shortNameEdit, &QLineEdit::editingFinished, this, &PositionPanel::onFieldsEdited);
    form->addRow(tr("Short name:"), m_shortNameEdit);

    m_delaySpin = new QDoubleSpinBox(editorGroup);
    m_delaySpin->setRange(0.0, 2000.0);
    m_delaySpin->setDecimals(1);
    m_delaySpin->setSingleStep(1.0);
    m_delaySpin->setSuffix(tr(" ms"));
    m_delaySpin->setToolTip(tr("Image-shift / time-alignment delay"));
    connect(m_delaySpin, &QDoubleSpinBox::editingFinished, this, &PositionPanel::onFieldsEdited);
    form->addRow(tr("Delay:"), m_delaySpin);

    m_panSpin = new QDoubleSpinBox(editorGroup);
    m_panSpin->setRange(-1.0, 1.0);
    m_panSpin->setDecimals(2);
    m_panSpin->setSingleStep(0.05);
    m_panSpin->setToolTip(tr("Pan: -1 full left, 0 center, +1 full right"));
    connect(m_panSpin, &QDoubleSpinBox::editingFinished, this, &PositionPanel::onFieldsEdited);
    form->addRow(tr("Pan:"), m_panSpin);

    m_busesEdit = new QLineEdit(editorGroup);
    m_busesEdit->setPlaceholderText(tr("e.g. 1, 3, 5"));
    m_busesEdit->setToolTip(tr("Buses this position images to; empty = main only"));
    connect(m_busesEdit, &QLineEdit::editingFinished, this, &PositionPanel::onFieldsEdited);
    form->addRow(tr("Buses:"), m_busesEdit);

    mainLayout->addWidget(editorGroup);
}

void PositionPanel::refresh() {
    if (m_app && m_app->show())
        m_library = m_app->show()->positionLibrary();

    populateList();
    updateEditor();
}

void PositionPanel::populateList() {
    const QString keepId = currentPositionId();

    m_updatingUi = true;
    m_list->clear();
    if (m_library) {
        for (const Position& p : m_library->positions()) {
            QListWidgetItem* item = new QListWidgetItem(itemText(p), m_list);
            item->setData(PositionIdRole, p.id());
            if (p.id() == keepId)
                m_list->setCurrentItem(item);
        }
    }
    m_updatingUi = false;
}

QString PositionPanel::currentPositionId() const {
    QListWidgetItem* item = m_list->currentItem();
    return item ? item->data(PositionIdRole).toString() : QString();
}

void PositionPanel::setEditorEnabled(bool enabled) {
    m_nameEdit->setEnabled(enabled);
    m_shortNameEdit->setEnabled(enabled);
    m_delaySpin->setEnabled(enabled);
    m_panSpin->setEnabled(enabled);
    m_busesEdit->setEnabled(enabled);
    m_removeButton->setEnabled(enabled);
}

void PositionPanel::updateEditor() {
    std::optional<Position> p =
        m_library ? m_library->position(currentPositionId()) : std::nullopt;

    m_updatingUi = true;
    if (p) {
        m_nameEdit->setText(p->name());
        m_shortNameEdit->setText(p->shortName());
        m_delaySpin->setValue(p->delay());
        m_panSpin->setValue(p->pan());
        m_busesEdit->setText(formatBuses(p->buses()));
        setEditorEnabled(true);
    } else {
        m_nameEdit->clear();
        m_shortNameEdit->clear();
        m_delaySpin->setValue(0.0);
        m_panSpin->setValue(0.0);
        m_busesEdit->clear();
        setEditorEnabled(false);
    }
    m_updatingUi = false;
}

void PositionPanel::addPosition() {
    if (!m_library)
        return;
    Position p(tr("New Position"));
    m_library->addPosition(p);

    QListWidgetItem* item = new QListWidgetItem(itemText(p), m_list);
    item->setData(PositionIdRole, p.id());
    m_list->setCurrentItem(item);
    updateEditor();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void PositionPanel::removePosition() {
    if (!m_library)
        return;
    const QString id = currentPositionId();
    if (id.isEmpty())
        return;
    m_library->removePosition(id);
    delete m_list->takeItem(m_list->currentRow());
    updateEditor();
}

void PositionPanel::onSelectionChanged() {
    if (m_updatingUi)
        return;
    updateEditor();
}

void PositionPanel::onFieldsEdited() {
    if (m_updatingUi)
        return;
    commitField();
}

void PositionPanel::commitField() {
    if (!m_library)
        return;
    std::optional<Position> current = m_library->position(currentPositionId());
    if (!current)
        return;

    Position p = *current;
    p.setName(m_nameEdit->text());
    p.setShortName(m_shortNameEdit->text());
    p.setDelay(m_delaySpin->value());
    p.setPan(m_panSpin->value());
    p.setBuses(parseBuses(m_busesEdit->text()));
    m_library->updatePosition(p);

    m_updatingUi = true;
    m_busesEdit->setText(formatBuses(p.buses())); // reflect normalization
    m_updatingUi = false;
    if (QListWidgetItem* item = m_list->currentItem())
        item->setText(itemText(p));
}

} // namespace OpenMix
