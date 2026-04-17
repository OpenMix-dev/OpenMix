#pragma once

#include "midi/MidiControlMapping.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

namespace OpenMix {

class MidiInputManager;

class MidiConfigDialog : public QDialog {
    Q_OBJECT

  public:
    explicit MidiConfigDialog(MidiInputManager* manager, QWidget* parent = nullptr);
    ~MidiConfigDialog() override;

  private slots:
    void onDeviceListChanged();
    void onDeviceChanged(int index);
    void onRefreshDevicesClicked();
    void onEnabledChanged(Qt::CheckState state);
    void onAutoReconnectChanged(Qt::CheckState state);
    void onAddMappingClicked();
    void onMidiLearnClicked();
    void onRemoveMappingClicked();
    void onMidiLearnReceived(const MidiTrigger& trigger);
    void onMappingActionChanged(int row);
    void accept() override;

  private:
    void setupUi();
    void populateDeviceList();
    void populateMappingsTable();
    void updateDeviceStatus();
    void addMappingRow(const MidiMapping& mapping);

    MidiInputManager* m_manager;

    // device section
    QComboBox* m_deviceCombo;
    QPushButton* m_refreshButton;
    QLabel* m_deviceStatusLabel;
    QCheckBox* m_enabledCheck;
    QCheckBox* m_autoReconnectCheck;

    // mappings section
    QTableWidget* m_mappingsTable;
    QPushButton* m_addMappingButton;
    QPushButton* m_midiLearnButton;

    // dialog buttons
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    // MIDI Learn state
    bool m_midiLearnActive = false;
    int m_midiLearnRow = -1;

    // pending changes
    QVector<MidiMapping> m_pendingMappings;
    QString m_pendingDeviceName;
    bool m_pendingEnabled = true;
    bool m_pendingAutoReconnect = true;
};

} // namespace OpenMix
