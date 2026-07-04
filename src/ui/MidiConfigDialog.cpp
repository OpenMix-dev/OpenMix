#include "MidiConfigDialog.h"
#include "WindowSizing.h"
#include "midi/MidiInputManager.h"
#include "theme/Icons.h"
#include "theme/Theme.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace OpenMix {

MidiConfigDialog::MidiConfigDialog(MidiInputManager* manager, int channelCount, QWidget* parent)
    : QDialog(parent), m_manager(manager), m_channelCount(qMax(1, channelCount)) {
    setWindowTitle(tr("MIDI Controller"));
    setMinimumWidth(600);
    WindowSizing::widenOnShow(this);

    // load current settings
    m_pendingMappings = manager->mappings();
    m_pendingMutes = manager->muteAssignments();
    m_pendingDeviceName = manager->currentDeviceName();
    m_pendingEnabled = manager->isEnabled();
    m_pendingAutoReconnect = manager->isAutoReconnectEnabled();

    setupUi();
    populateDeviceList();
    populateMappingsTable();
    populateMutesTable();
    updateDeviceStatus();

    connect(m_manager, &MidiInputManager::deviceListChanged, this,
            &MidiConfigDialog::onDeviceListChanged);
    connect(m_manager, &MidiInputManager::midiLearnReceived, this,
            &MidiConfigDialog::onMidiLearnReceived);
}

MidiConfigDialog::~MidiConfigDialog() {
    if (m_learnTarget != LearnTarget::None) {
        m_manager->stopMidiLearn();
    }
}

void MidiConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(Theme::Spacing::L, Theme::Spacing::L, Theme::Spacing::L,
                                   Theme::Spacing::L);
    mainLayout->setSpacing(Theme::Spacing::M);

    // device section
    QGroupBox* deviceGroup = new QGroupBox(tr("MIDI Device"), this);
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceGroup);
    deviceLayout->setSpacing(Theme::Spacing::S);

    QHBoxLayout* deviceSelectLayout = new QHBoxLayout();
    deviceSelectLayout->setSpacing(Theme::Spacing::S);
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MidiConfigDialog::onDeviceChanged);
    deviceSelectLayout->addWidget(m_deviceCombo);

    m_refreshButton = new QPushButton(Icons::refresh(), tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this,
            &MidiConfigDialog::onRefreshDevicesClicked);
    deviceSelectLayout->addWidget(m_refreshButton);
    deviceLayout->addLayout(deviceSelectLayout);

    m_deviceStatusLabel = new QLabel(this);
    deviceLayout->addWidget(m_deviceStatusLabel);

    QHBoxLayout* optionsLayout = new QHBoxLayout();
    optionsLayout->setSpacing(Theme::Spacing::M);
    m_enabledCheck = new QCheckBox(tr("Enable MIDI input"), this);
    m_enabledCheck->setChecked(m_pendingEnabled);
    connect(m_enabledCheck, &QCheckBox::checkStateChanged, this,
            &MidiConfigDialog::onEnabledChanged);
    optionsLayout->addWidget(m_enabledCheck);

    m_autoReconnectCheck = new QCheckBox(tr("Auto-reconnect"), this);
    m_autoReconnectCheck->setChecked(m_pendingAutoReconnect);
    m_autoReconnectCheck->setToolTip(tr("Automatically reconnect when device is plugged in"));
    connect(m_autoReconnectCheck, &QCheckBox::checkStateChanged, this,
            &MidiConfigDialog::onAutoReconnectChanged);
    optionsLayout->addWidget(m_autoReconnectCheck);
    optionsLayout->addStretch();
    deviceLayout->addLayout(optionsLayout);

    mainLayout->addWidget(deviceGroup);

    // action mappings section
    QGroupBox* mappingsGroup = new QGroupBox(tr("Action Mappings"), this);
    QVBoxLayout* mappingsLayout = new QVBoxLayout(mappingsGroup);
    mappingsLayout->setSpacing(Theme::Spacing::S);

    m_mappingsTable = new QTableWidget(this);
    m_mappingsTable->setColumnCount(3);
    m_mappingsTable->setHorizontalHeaderLabels({tr("MIDI Trigger"), tr("Action"), QString()});
    m_mappingsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mappingsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_mappingsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_mappingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mappingsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mappingsTable->verticalHeader()->setVisible(false);
    m_mappingsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mappingsTable->setMinimumHeight(140);
    // size to the actual rows so empty tables don't open with a dead band
    m_mappingsTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    mappingsLayout->addWidget(m_mappingsTable);

    QHBoxLayout* mappingButtonsLayout = new QHBoxLayout();
    mappingButtonsLayout->setSpacing(Theme::Spacing::S);
    mappingButtonsLayout->addStretch();
    m_addMappingButton = new QPushButton(Icons::listAdd(), tr("Add Mapping"), this);
    connect(m_addMappingButton, &QPushButton::clicked, this,
            &MidiConfigDialog::onAddMappingClicked);
    mappingButtonsLayout->addWidget(m_addMappingButton);

    m_midiLearnButton = new QPushButton(tr("MIDI Learn"), this);
    m_midiLearnButton->setToolTip(
        tr("Click, then press a button/key on your MIDI controller. With a row "
           "selected, its trigger is replaced; otherwise a new mapping is added."));
    connect(m_midiLearnButton, &QPushButton::clicked, this, &MidiConfigDialog::onMidiLearnClicked);
    mappingButtonsLayout->addWidget(m_midiLearnButton);
    mappingsLayout->addLayout(mappingButtonsLayout);

    mainLayout->addWidget(mappingsGroup);

    // channel mute buttons section
    QGroupBox* mutesGroup = new QGroupBox(tr("Channel Mute Buttons"), this);
    QVBoxLayout* mutesLayout = new QVBoxLayout(mutesGroup);
    mutesLayout->setSpacing(Theme::Spacing::S);

    QLabel* mutesHint = new QLabel(
        tr("Each trigger toggles the mute of one mixer input channel."), this);
    mutesHint->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    mutesLayout->addWidget(mutesHint);

    m_mutesTable = new QTableWidget(this);
    m_mutesTable->setColumnCount(3);
    m_mutesTable->setHorizontalHeaderLabels({tr("MIDI Trigger"), tr("Channel"), QString()});
    m_mutesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mutesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_mutesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_mutesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mutesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mutesTable->verticalHeader()->setVisible(false);
    m_mutesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mutesTable->setMinimumHeight(140);
    // size to the actual rows so empty tables don't open with a dead band
    m_mutesTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    mutesLayout->addWidget(m_mutesTable);

    QHBoxLayout* muteButtonsLayout = new QHBoxLayout();
    muteButtonsLayout->setSpacing(Theme::Spacing::S);
    muteButtonsLayout->addStretch();
    m_addMuteButton = new QPushButton(Icons::listAdd(), tr("Add Assignment"), this);
    connect(m_addMuteButton, &QPushButton::clicked, this, &MidiConfigDialog::onAddMuteClicked);
    muteButtonsLayout->addWidget(m_addMuteButton);

    m_muteLearnButton = new QPushButton(tr("MIDI Learn"), this);
    m_muteLearnButton->setToolTip(
        tr("Click, then press a button/key on your MIDI controller. With a row "
           "selected, its trigger is replaced; otherwise a new assignment is added."));
    connect(m_muteLearnButton, &QPushButton::clicked, this, &MidiConfigDialog::onMuteLearnClicked);
    muteButtonsLayout->addWidget(m_muteLearnButton);
    mutesLayout->addLayout(muteButtonsLayout);

    mainLayout->addWidget(mutesGroup);

    // learn status + dialog buttons
    m_learnStatusLabel = new QLabel(this);
    m_learnStatusLabel->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    mainLayout->addWidget(m_learnStatusLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &MidiConfigDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
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

void MidiConfigDialog::populateMutesTable() {
    m_mutesTable->setRowCount(0);
    for (const MidiMuteAssignment& assignment : m_pendingMutes) {
        addMuteRow(assignment);
    }
}

int MidiConfigDialog::rowOfCellWidget(QTableWidget* table, int column, QWidget* widget) const {
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->cellWidget(r, column) == widget)
            return r;
    }
    return -1;
}

bool MidiConfigDialog::triggerInUse(const MidiTrigger& trigger, int excludeActionRow,
                                    int excludeMuteRow) const {
    for (int i = 0; i < m_pendingMappings.size(); ++i) {
        if (i != excludeActionRow && m_pendingMappings[i].trigger == trigger)
            return true;
    }
    for (int i = 0; i < m_pendingMutes.size(); ++i) {
        if (i != excludeMuteRow && m_pendingMutes[i].trigger == trigger)
            return true;
    }
    return false;
}

void MidiConfigDialog::addMappingRow(const MidiMapping& mapping) {
    int row = m_mappingsTable->rowCount();
    m_mappingsTable->insertRow(row);

    // trigger column
    QTableWidgetItem* triggerItem = new QTableWidgetItem(mapping.trigger.toString());
    m_mappingsTable->setItem(row, 0, triggerItem);

    // action combo — resolve the row from the widget at call time; a captured
    // index goes stale as soon as a row above it is removed
    QComboBox* actionCombo = new QComboBox();
    QVector<MidiAction> actions = allMidiActions();
    for (MidiAction action : actions) {
        actionCombo->addItem(midiActionDisplayName(action), static_cast<int>(action));
    }
    actionCombo->setCurrentIndex(actionCombo->findData(static_cast<int>(mapping.action)));
    connect(actionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, actionCombo]() {
                onMappingActionChanged(rowOfCellWidget(m_mappingsTable, 1, actionCombo));
            });
    m_mappingsTable->setCellWidget(row, 1, actionCombo);

    // remove button
    auto* removeBtn = new QToolButton();
    removeBtn->setIcon(Icons::listRemove());
    removeBtn->setAutoRaise(true);
    removeBtn->setFixedSize(24, 24);
    removeBtn->setToolTip(tr("Remove"));
    connect(removeBtn, &QToolButton::clicked, this, [this, removeBtn]() {
        const int r = rowOfCellWidget(m_mappingsTable, 2, removeBtn);
        if (r >= 0 && r < m_pendingMappings.size()) {
            m_pendingMappings.removeAt(r);
            m_mappingsTable->removeRow(r);
        }
    });
    m_mappingsTable->setCellWidget(row, 2, removeBtn);
}

void MidiConfigDialog::addMuteRow(const MidiMuteAssignment& assignment) {
    int row = m_mutesTable->rowCount();
    m_mutesTable->insertRow(row);

    QTableWidgetItem* triggerItem = new QTableWidgetItem(assignment.trigger.toString());
    m_mutesTable->setItem(row, 0, triggerItem);

    auto* channelSpin = new QSpinBox();
    channelSpin->setRange(1, m_channelCount);
    channelSpin->setPrefix(tr("Ch "));
    channelSpin->setValue(qBound(1, assignment.channel, m_channelCount));
    connect(channelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, channelSpin]() {
        onMuteChannelChanged(rowOfCellWidget(m_mutesTable, 1, channelSpin));
    });
    m_mutesTable->setCellWidget(row, 1, channelSpin);

    auto* removeBtn = new QToolButton();
    removeBtn->setIcon(Icons::listRemove());
    removeBtn->setAutoRaise(true);
    removeBtn->setFixedSize(24, 24);
    removeBtn->setToolTip(tr("Remove"));
    connect(removeBtn, &QToolButton::clicked, this, [this, removeBtn]() {
        const int r = rowOfCellWidget(m_mutesTable, 2, removeBtn);
        if (r >= 0 && r < m_pendingMutes.size()) {
            m_pendingMutes.removeAt(r);
            m_mutesTable->removeRow(r);
        }
    });
    m_mutesTable->setCellWidget(row, 2, removeBtn);
}

void MidiConfigDialog::updateDeviceStatus() {
    if (m_manager->isDeviceOpen()) {
        m_deviceStatusLabel->setText(
            tr("Status: Connected to %1").arg(m_manager->currentDeviceName()));
        m_deviceStatusLabel->setStyleSheet(
            QString("color: %1;").arg(Theme::Colors::AccentGreen));
    } else if (!m_pendingDeviceName.isEmpty()) {
        m_deviceStatusLabel->setText(tr("Status: Device not available"));
        m_deviceStatusLabel->setStyleSheet(
            QString("color: %1;").arg(Theme::Colors::AccentAmber));
    } else {
        m_deviceStatusLabel->setText(tr("Status: No device selected"));
        m_deviceStatusLabel->setStyleSheet(
            QString("color: %1;").arg(Theme::Colors::TextSecondary));
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

void MidiConfigDialog::onEnabledChanged(Qt::CheckState state) {
    m_pendingEnabled = (state == Qt::Checked);
}

void MidiConfigDialog::onAutoReconnectChanged(Qt::CheckState state) {
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

void MidiConfigDialog::onAddMuteClicked() {
    MidiMuteAssignment assignment;
    assignment.trigger.type = MidiMessageType::NoteOn;
    assignment.trigger.channel = 0;
    assignment.trigger.noteOrCC = 0;
    assignment.trigger.minValue = 1;
    assignment.channel = 1;

    m_pendingMutes.append(assignment);
    addMuteRow(assignment);
}

void MidiConfigDialog::onMidiLearnClicked() {
    if (m_learnTarget == LearnTarget::Action) {
        stopLearn();
        return;
    }
    startLearn(LearnTarget::Action);
}

void MidiConfigDialog::onMuteLearnClicked() {
    if (m_learnTarget == LearnTarget::Mute) {
        stopLearn();
        return;
    }
    startLearn(LearnTarget::Mute);
}

void MidiConfigDialog::startLearn(LearnTarget target) {
    if (target == LearnTarget::None)
        return;

    if (!m_manager->isDeviceOpen()) {
        QMessageBox::warning(this, tr("MIDI Learn"),
                             tr("Please select and connect a MIDI device first."));
        return;
    }

    stopLearn(); // disarm the other section first

    m_learnTarget = target;
    QTableWidget* table = (target == LearnTarget::Action) ? m_mappingsTable : m_mutesTable;
    m_learnRow = table->currentRow(); // -1 = append a new row on receive

    QPushButton* armed = (target == LearnTarget::Action) ? m_midiLearnButton : m_muteLearnButton;
    QPushButton* other = (target == LearnTarget::Action) ? m_muteLearnButton : m_midiLearnButton;
    armed->setText(tr("Listening..."));
    armed->setStyleSheet(QString("background-color: %1; color: #131416; font-weight: 600;")
                             .arg(Theme::Colors::AccentAmber));
    other->setEnabled(false);

    m_learnStatusLabel->setText(
        tr("Press a control on %1... (click the Learn button again to cancel)")
            .arg(m_manager->currentDeviceName()));

    m_manager->startMidiLearn();
}

void MidiConfigDialog::stopLearn() {
    if (m_learnTarget != LearnTarget::None)
        m_manager->stopMidiLearn();

    m_learnTarget = LearnTarget::None;
    m_learnRow = -1;
    m_midiLearnButton->setText(tr("MIDI Learn"));
    m_midiLearnButton->setStyleSheet(QString());
    m_midiLearnButton->setEnabled(true);
    m_muteLearnButton->setText(tr("MIDI Learn"));
    m_muteLearnButton->setStyleSheet(QString());
    m_muteLearnButton->setEnabled(true);
    m_learnStatusLabel->clear();
}

void MidiConfigDialog::onMidiLearnReceived(const MidiTrigger& trigger) {
    const LearnTarget target = m_learnTarget;
    const int learnRow = m_learnRow;
    stopLearn();

    if (target == LearnTarget::None)
        return;

    // conflict check against the dialog's pending edits, not the manager's
    // committed lists (rows may have been added/removed/re-learned)
    const int excludeAction = (target == LearnTarget::Action) ? learnRow : -1;
    const int excludeMute = (target == LearnTarget::Mute) ? learnRow : -1;
    if (triggerInUse(trigger, excludeAction, excludeMute)) {
        QMessageBox::warning(this, tr("MIDI Learn"),
                             tr("This MIDI trigger is already assigned."));
        return;
    }

    if (target == LearnTarget::Action) {
        if (learnRow >= 0 && learnRow < m_pendingMappings.size()) {
            m_pendingMappings[learnRow].trigger = trigger;
            m_mappingsTable->item(learnRow, 0)->setText(trigger.toString());
        } else {
            MidiMapping mapping;
            mapping.trigger = trigger;
            mapping.action = MidiAction::Go; // default action
            m_pendingMappings.append(mapping);
            addMappingRow(mapping);
        }
    } else {
        if (learnRow >= 0 && learnRow < m_pendingMutes.size()) {
            m_pendingMutes[learnRow].trigger = trigger;
            m_mutesTable->item(learnRow, 0)->setText(trigger.toString());
        } else {
            MidiMuteAssignment assignment;
            assignment.trigger = trigger;
            assignment.channel = 1;
            m_pendingMutes.append(assignment);
            addMuteRow(assignment);
        }
    }
}

void MidiConfigDialog::onMappingActionChanged(int row) {
    if (row < 0 || row >= m_pendingMappings.size())
        return;

    QComboBox* combo = qobject_cast<QComboBox*>(m_mappingsTable->cellWidget(row, 1));
    if (!combo)
        return;

    m_pendingMappings[row].action = static_cast<MidiAction>(combo->currentData().toInt());
}

void MidiConfigDialog::onMuteChannelChanged(int row) {
    if (row < 0 || row >= m_pendingMutes.size())
        return;

    QSpinBox* spin = qobject_cast<QSpinBox*>(m_mutesTable->cellWidget(row, 1));
    if (!spin)
        return;

    m_pendingMutes[row].channel = spin->value();
}

void MidiConfigDialog::accept() {
    // apply changes
    m_manager->setEnabled(m_pendingEnabled);
    m_manager->setAutoReconnect(m_pendingAutoReconnect);
    m_manager->setMappings(m_pendingMappings);
    m_manager->setMuteAssignments(m_pendingMutes);

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
