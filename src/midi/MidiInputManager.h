#pragma once

#include "MidiControlMapping.h"
#include "MtcParser.h"

#include <QObject>
#include <QTimer>
#include <memory>

class RtMidiIn;

namespace OpenMix {

class PlaybackEngine;
class PlaybackGuard;

struct MidiDeviceInfo {
    int index;
    QString name;
};

class MidiInputManager : public QObject {
    Q_OBJECT

  public:
    explicit MidiInputManager(QObject* parent = nullptr);
    ~MidiInputManager() override;

    void setPlaybackEngine(PlaybackEngine* engine) { m_engine = engine; }
    void setPlaybackGuard(PlaybackGuard* guard) { m_guard = guard; }

    // device management
    QVector<MidiDeviceInfo> availableDevices() const;
    bool openDevice(int deviceIndex);
    bool openDevice(const QString& deviceName);
    void closeDevice();
    bool isDeviceOpen() const { return m_deviceOpen; }
    QString currentDeviceName() const { return m_currentDeviceName; }

    // auto-reconnect
    void setAutoReconnect(bool enabled);
    bool isAutoReconnectEnabled() const { return m_autoReconnect; }

    // enable/disable
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // MIDI Show Control (cue GO/STOP from incoming sysex)
    void setMscEnabled(bool enabled) { m_mscEnabled = enabled; }
    bool isMscEnabled() const { return m_mscEnabled; }
    void setMscDeviceId(int id) { m_mscDeviceId = id; } // 0-127; 0x7F = all devices
    int mscDeviceId() const { return m_mscDeviceId; }

    // mapping management
    void setMappings(const QVector<MidiMapping>& mappings);
    QVector<MidiMapping> mappings() const { return m_mappings; }
    void addMapping(const MidiMapping& mapping);
    void removeMapping(int index);
    void clearMappings();
    bool hasConflict(const MidiTrigger& trigger, int excludeIndex = -1) const;

    // channel mute-button assignments (trigger -> channel mute toggle)
    void setMuteAssignments(const QVector<MidiMuteAssignment>& assignments);
    QVector<MidiMuteAssignment> muteAssignments() const { return m_muteAssignments; }
    void addMuteAssignment(const MidiMuteAssignment& assignment);
    void clearMuteAssignments();

    // MIDI Learn mode
    void startMidiLearn();
    void stopMidiLearn();
    bool isMidiLearnActive() const { return m_midiLearnActive; }

    // persistence
    void saveToSettings();
    void loadFromSettings();

  signals:
    void deviceListChanged();
    void deviceOpened(const QString& deviceName);
    void deviceClosed();
    void deviceError(const QString& error);
    void mappingsChanged();
    void midiMessageReceived(MidiMessageType type, int channel, int noteOrCC, int value);
    void midiLearnReceived(const MidiTrigger& trigger);
    void actionExecuted(MidiAction action);
    void channelMuteToggled(int channel); // a mute-button assignment fired
    void mscReceived(int command); // a MIDI Show Control command was handled
    // a full MIDI Time Code frame was assembled (from quarter-frame or full-frame)
    void timecodeChanged(int hours, int minutes, int seconds, int frames);

  private slots:
    void onDevicePollTimer();

  private:

    void processMidiMessage(const std::vector<unsigned char>& message);
    void handleSysex(const std::vector<unsigned char>& message);
    void executeAction(MidiAction action);
    bool tryReconnect();
    int findDeviceIndex(const QString& deviceName) const;

    static void midiCallback(double timeStamp, std::vector<unsigned char>* message, void* userData);

    std::unique_ptr<RtMidiIn> m_midiIn;
    bool m_deviceOpen = false;
    QString m_currentDeviceName;
    QString m_savedDeviceName;

    bool m_enabled = true;
    bool m_autoReconnect = true;
    bool m_mscEnabled = true;
    int m_mscDeviceId = 0x7F; // 0x7F = respond to all device IDs

    QVector<MidiMapping> m_mappings;
    QVector<MidiMuteAssignment> m_muteAssignments;

    bool m_midiLearnActive = false;

    QTimer m_devicePollTimer;
    QStringList m_lastDeviceList;

    MtcParser m_mtcParser; // assembles MIDI Time Code quarter-frames

    PlaybackEngine* m_engine = nullptr;
    PlaybackGuard* m_guard = nullptr;
};

} // namespace OpenMix
