#include "FxSetupDialog.h"
#include "WindowSizing.h"
#include "app/Application.h"
#include "core/FxLibrary.h"
#include "core/Show.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace OpenMix {

namespace {
QList<int> parseIntList(const QString& text) {
    QList<int> out;
    for (const QString& tok : text.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts)) {
        bool ok = false;
        int v = tok.toInt(&ok);
        if (ok)
            out.append(v);
    }
    return out;
}

QString joinInts(const QList<int>& list) {
    QStringList parts;
    for (int v : list)
        parts.append(QString::number(v));
    return parts.join(", ");
}
} // namespace

FxSetupDialog::FxSetupDialog(Application* app, QWidget* parent) : QDialog(parent), m_app(app) {
    setWindowTitle(tr("FX Setup"));
    setModal(true);
    resize(460, 520);
    WindowSizing::widenOnShow(this);
    setupUi();
    load();
}

void FxSetupDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* unitsGroup = new QGroupBox(tr("FX Units"), this);
    QVBoxLayout* unitsLayout = new QVBoxLayout(unitsGroup);
    m_unitsTable = new QTableWidget(0, 2, unitsGroup);
    m_unitsTable->setHorizontalHeaderLabels({tr("Index"), tr("Name")});
    m_unitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_unitsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_unitsTable->verticalHeader()->setVisible(false);
    unitsLayout->addWidget(m_unitsTable);
    QHBoxLayout* unitsButtons = new QHBoxLayout();
    QPushButton* addUnit = new QPushButton(tr("Add"), unitsGroup);
    connect(addUnit, &QPushButton::clicked, this, &FxSetupDialog::addUnitRow);
    QPushButton* removeUnit = new QPushButton(tr("Remove"), unitsGroup);
    connect(removeUnit, &QPushButton::clicked, this, &FxSetupDialog::removeUnitRow);
    unitsButtons->addStretch();
    unitsButtons->addWidget(addUnit);
    unitsButtons->addWidget(removeUnit);
    unitsLayout->addLayout(unitsButtons);
    mainLayout->addWidget(unitsGroup);

    QGroupBox* assignGroup = new QGroupBox(tr("Channel Assignments"), this);
    QVBoxLayout* assignLayout = new QVBoxLayout(assignGroup);
    m_assignTable = new QTableWidget(0, 2, assignGroup);
    m_assignTable->setHorizontalHeaderLabels({tr("Channel"), tr("FX Units")});
    m_assignTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_assignTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_assignTable->verticalHeader()->setVisible(false);
    assignLayout->addWidget(m_assignTable);
    QHBoxLayout* assignButtons = new QHBoxLayout();
    QPushButton* addAssign = new QPushButton(tr("Add"), assignGroup);
    connect(addAssign, &QPushButton::clicked, this, &FxSetupDialog::addAssignRow);
    QPushButton* removeAssign = new QPushButton(tr("Remove"), assignGroup);
    connect(removeAssign, &QPushButton::clicked, this, &FxSetupDialog::removeAssignRow);
    assignButtons->addStretch();
    assignButtons->addWidget(addAssign);
    assignButtons->addWidget(removeAssign);
    assignLayout->addLayout(assignButtons);
    mainLayout->addWidget(assignGroup);

    QHBoxLayout* defaultRow = new QHBoxLayout();
    defaultRow->addWidget(new QLabel(tr("Default FX unit:"), this));
    m_defaultSpin = new QSpinBox(this);
    m_defaultSpin->setRange(-1, 128);
    m_defaultSpin->setSpecialValueText(tr("None"));
    defaultRow->addWidget(m_defaultSpin);
    defaultRow->addStretch();
    mainLayout->addLayout(defaultRow);

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &FxSetupDialog::applyChanges);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void FxSetupDialog::load() {
    if (!m_app || !m_app->show())
        return;
    const FxLibrary* fx = m_app->show()->fxLibrary();

    for (const FxUnit& unit : fx->units()) {
        const int row = m_unitsTable->rowCount();
        m_unitsTable->insertRow(row);
        m_unitsTable->setItem(row, 0, new QTableWidgetItem(QString::number(unit.index)));
        m_unitsTable->setItem(row, 1, new QTableWidgetItem(unit.name));
    }

    const QMap<int, QList<int>> assigns = fx->assignments();
    for (auto it = assigns.begin(); it != assigns.end(); ++it) {
        const int row = m_assignTable->rowCount();
        m_assignTable->insertRow(row);
        m_assignTable->setItem(row, 0, new QTableWidgetItem(QString::number(it.key())));
        m_assignTable->setItem(row, 1, new QTableWidgetItem(joinInts(it.value())));
    }

    m_defaultSpin->setValue(fx->defaultFx());
}

void FxSetupDialog::addUnitRow() {
    const int row = m_unitsTable->rowCount();
    m_unitsTable->insertRow(row);
    m_unitsTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    m_unitsTable->setItem(row, 1, new QTableWidgetItem());
    m_unitsTable->editItem(m_unitsTable->item(row, 1));
}

void FxSetupDialog::removeUnitRow() {
    const int row = m_unitsTable->currentRow();
    if (row >= 0)
        m_unitsTable->removeRow(row);
}

void FxSetupDialog::addAssignRow() {
    const int row = m_assignTable->rowCount();
    m_assignTable->insertRow(row);
    m_assignTable->setItem(row, 0, new QTableWidgetItem());
    m_assignTable->setItem(row, 1, new QTableWidgetItem());
    m_assignTable->editItem(m_assignTable->item(row, 0));
}

void FxSetupDialog::removeAssignRow() {
    const int row = m_assignTable->currentRow();
    if (row >= 0)
        m_assignTable->removeRow(row);
}

void FxSetupDialog::applyChanges() {
    if (!m_app || !m_app->show()) {
        accept();
        return;
    }
    FxLibrary* fx = m_app->show()->fxLibrary();
    fx->clear();

    for (int row = 0; row < m_unitsTable->rowCount(); ++row) {
        QTableWidgetItem* indexItem = m_unitsTable->item(row, 0);
        QTableWidgetItem* nameItem = m_unitsTable->item(row, 1);
        if (!indexItem)
            continue;
        bool ok = false;
        const int index = indexItem->text().toInt(&ok);
        if (ok)
            fx->setUnit(index, nameItem ? nameItem->text() : QString());
    }

    for (int row = 0; row < m_assignTable->rowCount(); ++row) {
        QTableWidgetItem* channelItem = m_assignTable->item(row, 0);
        QTableWidgetItem* unitsItem = m_assignTable->item(row, 1);
        if (!channelItem)
            continue;
        bool ok = false;
        const int channel = channelItem->text().toInt(&ok);
        if (ok && unitsItem)
            fx->setChannelAssignment(channel, parseIntList(unitsItem->text()));
    }

    fx->setDefaultFx(m_defaultSpin->value());
    accept();
}

} // namespace OpenMix
