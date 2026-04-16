#pragma once

#include "MidiControlMapping.h"

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

    // mapping management
    void setMappings(const QVector<MidiMapping>& mappings);
    QVector<MidiMapping> mappings() const { return m_mappings; }
    void addMapping(const MidiMapping& mapping);
    void removeMapping(int index);
    void clearMappings();
    bool hasConflict(const MidiTrigger& trigger, int excludeIndex = -1) const;

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

  private slots:
    void onDevicePollTimer();

  private:
    void processMidiMessage(const std::vector<unsigned char>& message);
    void executeAction(MidiAction action);
    bool tryReconnect();

    static void midiCallback(double timeStamp, std::vector<unsigned char>* message, void* userData);

    std::unique_ptr<RtMidiIn> m_midiIn;
    bool m_deviceOpen = false;
    QString m_currentDeviceName;
    QString m_savedDeviceName;

    bool m_enabled = true;
    bool m_autoReconnect = true;

    QVector<MidiMapping> m_mappings;

    bool m_midiLearnActive = false;

    QTimer m_devicePollTimer;
    QStringList m_lastDeviceList;

    PlaybackEngine* m_engine = nullptr;
    PlaybackGuard* m_guard = nullptr;
};

} // namespace OpenMix
