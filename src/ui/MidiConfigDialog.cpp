#include "MidiConfigDialog.h"
#include "midi/MidiInputManager.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QVBoxLayout>

namespace OpenMix {

MidiConfigDialog::MidiConfigDialog(MidiInputManager* manager, QWidget* parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle(tr("MIDI Controller Configuration"));
    setMinimumSize(550, 450);

    // load current settings
    m_pendingMappings = manager->mappings();
    m_pendingDeviceName = manager->currentDeviceName();
    m_pendingEnabled = manager->isEnabled();
    m_pendingAutoReconnect = manager->isAutoReconnectEnabled();

    setupUi();
    populateDeviceList();
    populateMappingsTable();
    updateDeviceStatus();

    connect(m_manager, &MidiInputManager::deviceListChanged, this,
            &MidiConfigDialog::onDeviceListChanged);
    connect(m_manager, &MidiInputManager::midiLearnReceived, this,
            &MidiConfigDialog::onMidiLearnReceived);
}

MidiConfigDialog::~MidiConfigDialog() {
    if (m_midiLearnActive) {
        m_manager->stopMidiLearn();
    }
}

void MidiConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // device section
    QGroupBox* deviceGroup = new QGroupBox(tr("MIDI Device"), this);
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceGroup);

    QHBoxLayout* deviceSelectLayout = new QHBoxLayout();
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MidiConfigDialog::onDeviceChanged);
    deviceSelectLayout->addWidget(m_deviceCombo);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this,
            &MidiConfigDialog::onRefreshDevicesClicked);
    deviceSelectLayout->addWidget(m_refreshButton);
    deviceLayout->addLayout(deviceSelectLayout);

    m_deviceStatusLabel = new QLabel(this);
    deviceLayout->addWidget(m_deviceStatusLabel);

    QHBoxLayout* optionsLayout = new QHBoxLayout();
    m_enabledCheck = new QCheckBox(tr("Enable MIDI input"), this);
    m_enabledCheck->setChecked(m_pendingEnabled);
    connect(m_enabledCheck, &QCheckBox::stateChanged, this, &MidiConfigDialog::onEnabledChanged);
    optionsLayout->addWidget(m_enabledCheck);

    m_autoReconnectCheck = new QCheckBox(tr("Auto-reconnect"), this);
    m_autoReconnectCheck->setChecked(m_pendingAutoReconnect);
    m_autoReconnectCheck->setToolTip(tr("Automatically reconnect when device is plugged in"));
    connect(m_autoReconnectCheck, &QCheckBox::stateChanged, this,
            &MidiConfigDialog::onAutoReconnectChanged);
    optionsLayout->addWidget(m_autoReconnectCheck);
    optionsLayout->addStretch();
    deviceLayout->addLayout(optionsLayout);

    mainLayout->addWidget(deviceGroup);

    // mappings section
    QGroupBox* mappingsGroup = new QGroupBox(tr("Action Mappings"), this);
    QVBoxLayout* mappingsLayout = new QVBoxLayout(mappingsGroup);

    m_mappingsTable = new QTableWidget(this);
    m_mappingsTable->setColumnCount(3);
    m_mappingsTable->setHorizontalHeaderLabels({tr("MIDI Trigger"), tr("Action"), tr("")});
    m_mappingsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mappingsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_mappingsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_mappingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mappingsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mappingsTable->verticalHeader()->setVisible(false);
    m_mappingsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mappingsLayout->addWidget(m_mappingsTable);

    QHBoxLayout* mappingButtonsLayout = new QHBoxLayout();
    m_addMappingButton = new QPushButton(tr("Add Mapping"), this);
    connect(m_addMappingButton, &QPushButton::clicked, this,
            &MidiConfigDialog::onAddMappingClicked);
    mappingButtonsLayout->addWidget(m_addMappingButton);

    m_midiLearnButton = new QPushButton(tr("MIDI Learn"), this);
    m_midiLearnButton->setToolTip(tr("Click, then press a button/key on your MIDI controller"));
    connect(m_midiLearnButton, &QPushButton::clicked, this, &MidiConfigDialog::onMidiLearnClicked);
    mappingButtonsLayout->addWidget(m_midiLearnButton);

    mappingButtonsLayout->addStretch();
    mappingsLayout->addLayout(mappingButtonsLayout);

    mainLayout->addWidget(mappingsGroup);

    // dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &MidiConfigDialog::accept);
    buttonLayout->addWidget(m_okButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void MidiConfigDialog::populateDeviceList() {
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    m_deviceCombo->addItem(tr("(No device)"), -1);

    QVector<MidiDeviceInfo> devices = m_manager->availableDevices();
    int currentIndex = 0;

    for (int i = 0; i < devices.size(); ++i) {
        m_deviceCombo->addItem(devices[i].name, devices[i].index);
        if (devices[i].name == m_pendingDeviceName) {
            currentIndex = i + 1;
        }
    }

    m_deviceCombo->setCurrentIndex(currentIndex);
    m_deviceCombo->blockSignals(false);
}

void MidiConfigDialog::populateMappingsTable() {
    m_mappingsTable->setRowCount(0);
    for (const MidiMapping& mapping : m_pendingMappings) {
        addMappingRow(mapping);
    }
}

void MidiConfigDialog::addMappingRow(const MidiMapping& mapping) {
    int row = m_mappingsTable->rowCount();
    m_mappingsTable->insertRow(row);

    // trigger column
    QTableWidgetItem* triggerItem = new QTableWidgetItem(mapping.trigger.toString());
    m_mappingsTable->setItem(row, 0, triggerItem);

    // action combo
    QComboBox* actionCombo = new QComboBox();
    QVector<MidiAction> actions = allMidiActions();
    for (MidiAction action : actions) {
        actionCombo->addItem(midiActionDisplayName(action), static_cast<int>(action));
    }
    actionCombo->setCurrentIndex(actionCombo->findData(static_cast<int>(mapping.action)));
    connect(actionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, row]() { onMappingActionChanged(row); });
    m_mappingsTable->setCellWidget(row, 1, actionCombo);

    // remove button
    QPushButton* removeBtn = new QPushButton(tr("Remove"));
    connect(removeBtn, &QPushButton::clicked, this, [this, row]() {
        // find actual row since table may have changed
        for (int r = 0; r < m_mappingsTable->rowCount(); ++r) {
            if (m_mappingsTable->cellWidget(r, 2) == sender()) {
                m_pendingMappings.removeAt(r);
                m_mappingsTable->removeRow(r);
                break;
            }
        }
    });
    m_mappingsTable->setCellWidget(row, 2, removeBtn);
}

void MidiConfigDialog::updateDeviceStatus() {
    if (m_manager->isDeviceOpen()) {
        m_deviceStatusLabel->setText(
            tr("Status: Connected to %1").arg(m_manager->currentDeviceName()));
        m_deviceStatusLabel->setStyleSheet("color: green;");
    } else if (!m_pendingDeviceName.isEmpty()) {
        m_deviceStatusLabel->setText(tr("Status: Device not available"));
        m_deviceStatusLabel->setStyleSheet("color: orange;");
    } else {
        m_deviceStatusLabel->setText(tr("Status: No device selected"));
        m_deviceStatusLabel->setStyleSheet("");
    }
}

void MidiConfigDialog::onDeviceListChanged() { populateDeviceList(); }

void MidiConfigDialog::onDeviceChanged(int index) {
    if (index <= 0) {
        m_pendingDeviceName.clear();
    } else {
        m_pendingDeviceName = m_deviceCombo->itemText(index);
    }
    updateDeviceStatus();
}

void MidiConfigDialog::onRefreshDevicesClicked() { populateDeviceList(); }

void MidiConfigDialog::onEnabledChanged(int state) { m_pendingEnabled = (state == Qt::Checked); }

void MidiConfigDialog::onAutoReconnectChanged(int state) {
    m_pendingAutoReconnect = (state == Qt::Checked);
}

void MidiConfigDialog::onAddMappingClicked() {
    MidiMapping mapping;
    mapping.trigger.type = MidiMessageType::NoteOn;
    mapping.trigger.channel = 0;
    mapping.trigger.noteOrCC = 0;
    mapping.trigger.minValue = 1;
    mapping.action = MidiAction::Go;

    m_pendingMappings.append(mapping);
    addMappingRow(mapping);
}

void MidiConfigDialog::onMidiLearnClicked() {
    if (m_midiLearnActive) {
        m_manager->stopMidiLearn();
        m_midiLearnActive = false;
        m_midiLearnButton->setText(tr("MIDI Learn"));
        return;
    }

    if (!m_manager->isDeviceOpen()) {
        QMessageBox::warning(this, tr("MIDI Learn"),
                             tr("Please select and connect a MIDI device first."));
        return;
    }

    m_midiLearnActive = true;
    m_midiLearnRow = m_mappingsTable->rowCount(); // adds new row on receive
    m_midiLearnButton->setText(tr("Cancel Learn"));
    m_manager->startMidiLearn();
}

void MidiConfigDialog::onRemoveMappingClicked() {
    int row = m_mappingsTable->currentRow();
    if (row >= 0 && row < m_pendingMappings.size()) {
        m_pendingMappings.removeAt(row);
        m_mappingsTable->removeRow(row);
    }
}

void MidiConfigDialog::onMidiLearnReceived(const MidiTrigger& trigger) {
    m_midiLearnActive = false;
    m_midiLearnButton->setText(tr("MIDI Learn"));

    // check for conflict
    if (m_manager->hasConflict(trigger)) {
        QMessageBox::warning(this, tr("MIDI Learn"),
                             tr("This MIDI trigger is already assigned to another action."));
        return;
    }

    // add new mapping with learned trigger
    MidiMapping mapping;
    mapping.trigger = trigger;
    mapping.action = MidiAction::Go; // default action

    m_pendingMappings.append(mapping);
    addMappingRow(mapping);
}

void MidiConfigDialog::onMappingActionChanged(int row) {
    if (row < 0 || row >= m_pendingMappings.size())
        return;

    QComboBox* combo = qobject_cast<QComboBox*>(m_mappingsTable->cellWidget(row, 1));
    if (!combo)
        return;

    m_pendingMappings[row].action = static_cast<MidiAction>(combo->currentData().toInt());
}

void MidiConfigDialog::accept() {
    // apply changes
    m_manager->setEnabled(m_pendingEnabled);
    m_manager->setAutoReconnect(m_pendingAutoReconnect);
    m_manager->setMappings(m_pendingMappings);

    // handle device change
    if (m_pendingDeviceName != m_manager->currentDeviceName()) {
        if (m_pendingDeviceName.isEmpty()) {
            m_manager->closeDevice();
        } else {
            m_manager->openDevice(m_pendingDeviceName);
        }
    }

    m_manager->saveToSettings();
    QDialog::accept();
}

} // namespace OpenMix
