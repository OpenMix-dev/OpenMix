#include "CueZeroDialog.h"
#include "app/Application.h"
#include "core/CueZero.h"
#include "core/Show.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace OpenMix {

CueZeroDialog::CueZeroDialog(Application* app, QWidget* parent) : QDialog(parent), m_app(app) {
    setWindowTitle(tr("Cue Zero"));
    setModal(true);
    resize(480, 520);
    setupUi();
    load();
}

void CueZeroDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* intro = new QLabel(
        tr("Base/reset state recalled before the first cue. These values also "
           "serve as the panic safe-values."),
        this);
    intro->setWordWrap(true);
    mainLayout->addWidget(intro);

    // base scene
    QHBoxLayout* sceneLayout = new QHBoxLayout();
    sceneLayout->addWidget(new QLabel(tr("Base scene:"), this));
    m_baseSceneSpin = new QSpinBox(this);
    m_baseSceneSpin->setRange(-1, 9999);
    m_baseSceneSpin->setSpecialValueText(tr("None"));
    m_baseSceneSpin->setToolTip(tr("Scene recalled first; None to skip"));
    sceneLayout->addWidget(m_baseSceneSpin);
    sceneLayout->addStretch();
    mainLayout->addLayout(sceneLayout);

    // levels editor
    QGroupBox* levelsGroup = new QGroupBox(tr("Levels (path → value)"), this);
    QVBoxLayout* levelsLayout = new QVBoxLayout(levelsGroup);
    m_levelsTable = new QTableWidget(0, 2, levelsGroup);
    m_levelsTable->setHorizontalHeaderLabels({tr("Path"), tr("Value")});
    m_levelsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_levelsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_levelsTable->verticalHeader()->setVisible(false);
    levelsLayout->addWidget(m_levelsTable);
    QHBoxLayout* levelsButtons = new QHBoxLayout();
    QPushButton* addLevel = new QPushButton(tr("Add"), levelsGroup);
    connect(addLevel, &QPushButton::clicked, this, &CueZeroDialog::addLevelRow);
    QPushButton* removeLevel = new QPushButton(tr("Remove"), levelsGroup);
    connect(removeLevel, &QPushButton::clicked, this, &CueZeroDialog::removeLevelRow);
    levelsButtons->addStretch();
    levelsButtons->addWidget(addLevel);
    levelsButtons->addWidget(removeLevel);
    levelsLayout->addLayout(levelsButtons);
    mainLayout->addWidget(levelsGroup);

    // labels editor
    QGroupBox* labelsGroup = new QGroupBox(tr("Labels (path → name)"), this);
    QVBoxLayout* labelsLayout = new QVBoxLayout(labelsGroup);
    m_labelsTable = new QTableWidget(0, 2, labelsGroup);
    m_labelsTable->setHorizontalHeaderLabels({tr("Path"), tr("Name")});
    m_labelsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_labelsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_labelsTable->verticalHeader()->setVisible(false);
    labelsLayout->addWidget(m_labelsTable);
    QHBoxLayout* labelsButtons = new QHBoxLayout();
    QPushButton* addLabel = new QPushButton(tr("Add"), labelsGroup);
    connect(addLabel, &QPushButton::clicked, this, &CueZeroDialog::addLabelRow);
    QPushButton* removeLabel = new QPushButton(tr("Remove"), labelsGroup);
    connect(removeLabel, &QPushButton::clicked, this, &CueZeroDialog::removeLabelRow);
    labelsButtons->addStretch();
    labelsButtons->addWidget(addLabel);
    labelsButtons->addWidget(removeLabel);
    labelsLayout->addLayout(labelsButtons);
    mainLayout->addWidget(labelsGroup);

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CueZeroDialog::applyChanges);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void CueZeroDialog::load() {
    if (!m_app || !m_app->show())
        return;
    const CueZero* cueZero = m_app->show()->cueZero();

    m_baseSceneSpin->setValue(cueZero->baseScene());

    const QJsonObject levels = cueZero->levels();
    m_levelsTable->setRowCount(0);
    for (auto it = levels.begin(); it != levels.end(); ++it) {
        const int row = m_levelsTable->rowCount();
        m_levelsTable->insertRow(row);
        m_levelsTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_levelsTable->setItem(
            row, 1, new QTableWidgetItem(QString::number(it.value().toDouble())));
    }

    const QJsonObject labels = cueZero->labels();
    m_labelsTable->setRowCount(0);
    for (auto it = labels.begin(); it != labels.end(); ++it) {
        const int row = m_labelsTable->rowCount();
        m_labelsTable->insertRow(row);
        m_labelsTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_labelsTable->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
    }
}

void CueZeroDialog::addLevelRow() {
    const int row = m_levelsTable->rowCount();
    m_levelsTable->insertRow(row);
    m_levelsTable->setItem(row, 0, new QTableWidgetItem());
    m_levelsTable->setItem(row, 1, new QTableWidgetItem("0"));
    m_levelsTable->editItem(m_levelsTable->item(row, 0));
}

void CueZeroDialog::removeLevelRow() {
    const int row = m_levelsTable->currentRow();
    if (row >= 0)
        m_levelsTable->removeRow(row);
}

void CueZeroDialog::addLabelRow() {
    const int row = m_labelsTable->rowCount();
    m_labelsTable->insertRow(row);
    m_labelsTable->setItem(row, 0, new QTableWidgetItem());
    m_labelsTable->setItem(row, 1, new QTableWidgetItem());
    m_labelsTable->editItem(m_labelsTable->item(row, 0));
}

void CueZeroDialog::removeLabelRow() {
    const int row = m_labelsTable->currentRow();
    if (row >= 0)
        m_labelsTable->removeRow(row);
}

void CueZeroDialog::applyChanges() {
    if (!m_app || !m_app->show()) {
        accept();
        return;
    }
    CueZero* cueZero = m_app->show()->cueZero();

    cueZero->setBaseScene(m_baseSceneSpin->value());

    QJsonObject levels;
    for (int row = 0; row < m_levelsTable->rowCount(); ++row) {
        QTableWidgetItem* pathItem = m_levelsTable->item(row, 0);
        QTableWidgetItem* valueItem = m_levelsTable->item(row, 1);
        if (!pathItem || pathItem->text().trimmed().isEmpty())
            continue;
        const double value = valueItem ? valueItem->text().toDouble() : 0.0;
        levels.insert(pathItem->text().trimmed(), value);
    }
    cueZero->setLevels(levels);

    QJsonObject labels;
    for (int row = 0; row < m_labelsTable->rowCount(); ++row) {
        QTableWidgetItem* pathItem = m_labelsTable->item(row, 0);
        QTableWidgetItem* nameItem = m_labelsTable->item(row, 1);
        if (!pathItem || pathItem->text().trimmed().isEmpty())
            continue;
        labels.insert(pathItem->text().trimmed(), nameItem ? nameItem->text() : QString());
    }
    cueZero->setLabels(labels);

    accept();
}

} // namespace OpenMix
