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
    // channelCount bounds the mute-assignment channel selector (from the
    // selected/connected console's capabilities)
    explicit MidiConfigDialog(MidiInputManager* manager, int channelCount,
                              QWidget* parent = nullptr);
    ~MidiConfigDialog() override;

  private slots:
    void onDeviceListChanged();
    void onDeviceChanged(int index);
    void onRefreshDevicesClicked();
    void onEnabledChanged(Qt::CheckState state);
    void onAutoReconnectChanged(Qt::CheckState state);
    void onAddMappingClicked();
    void onMidiLearnClicked();
    void onAddMuteClicked();
    void onMuteLearnClicked();
    void onMidiLearnReceived(const MidiTrigger& trigger);
    void onMappingActionChanged(int row);
    void onMuteChannelChanged(int row);

  public slots:
    void accept() override; // public like QDialog's, and reachable from tests

  private:
    // which table an armed MIDI Learn feeds
    enum class LearnTarget { None, Action, Mute };

    void setupUi();
    void populateDeviceList();
    void populateMappingsTable();
    void populateMutesTable();
    void updateDeviceStatus();
    void addMappingRow(const MidiMapping& mapping);
    void addMuteRow(const MidiMuteAssignment& assignment);
    void startLearn(LearnTarget target);
    void stopLearn();
    // current row of the cell widget in the given table column, or -1 — lambdas
    // resolve their row at call time so removals never leave them stale
    int rowOfCellWidget(QTableWidget* table, int column, QWidget* widget) const;
    // conflict check against the dialog's pending state (not the manager's
    // committed lists); exclude*Row skip the row being re-learned
    bool triggerInUse(const MidiTrigger& trigger, int excludeActionRow, int excludeMuteRow) const;

    MidiInputManager* m_manager;
    int m_channelCount;

    // device section
    QComboBox* m_deviceCombo;
    QPushButton* m_refreshButton;
    QLabel* m_deviceStatusLabel;
    QCheckBox* m_enabledCheck;
    QCheckBox* m_autoReconnectCheck;

    // action mappings section
    QTableWidget* m_mappingsTable;
    QPushButton* m_addMappingButton;
    QPushButton* m_midiLearnButton;

    // channel mute buttons section
    QTableWidget* m_mutesTable;
    QPushButton* m_addMuteButton;
    QPushButton* m_muteLearnButton;

    QLabel* m_learnStatusLabel;

    // MIDI Learn state
    LearnTarget m_learnTarget = LearnTarget::None;
    int m_learnRow = -1; // row whose trigger is replaced; -1 appends

    // pending changes
    QVector<MidiMapping> m_pendingMappings;
    QVector<MidiMuteAssignment> m_pendingMutes;
    QString m_pendingDeviceName;
    bool m_pendingEnabled = true;
    bool m_pendingAutoReconnect = true;
};

} // namespace OpenMix
