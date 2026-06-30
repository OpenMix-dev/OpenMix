#include "MidiInputManager.h"
#include "MscParser.h"
#include "core/PlaybackEngine.h"
#include "core/PlaybackGuard.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <RtMidi.h>
#include <algorithm>

namespace OpenMix {

namespace {
constexpr int DEVICE_POLL_INTERVAL_MS = 2000;
} // namespace

MidiInputManager::MidiInputManager(QObject* parent) : QObject(parent) {
    try {
        m_midiIn = std::make_unique<RtMidiIn>();
    } catch (RtMidiError& error) {
        qWarning() << "RtMidi initialization error:" << QString::fromStdString(error.getMessage());
    }

    // device polling timer for hot-plug detection
    m_devicePollTimer.setInterval(DEVICE_POLL_INTERVAL_MS);
    connect(&m_devicePollTimer, &QTimer::timeout, this, &MidiInputManager::onDevicePollTimer);
    m_devicePollTimer.start();
}

MidiInputManager::~MidiInputManager() {
    closeDevice();
}

QVector<MidiDeviceInfo> MidiInputManager::availableDevices() const {
    QVector<MidiDeviceInfo> devices;
    if (!m_midiIn)
        return devices;

    try {
        unsigned int portCount = m_midiIn->getPortCount();
        for (unsigned int i = 0; i < portCount; ++i) {
            MidiDeviceInfo info;
            info.index = static_cast<int>(i);
            info.name = QString::fromStdString(m_midiIn->getPortName(i));
            devices.append(info);
        }
    } catch (RtMidiError& error) {
        qWarning() << "Error enumerating MIDI devices:"
                   << QString::fromStdString(error.getMessage());
    }

    return devices;
}

bool MidiInputManager::openDevice(int deviceIndex) {
    if (!m_midiIn)
        return false;

    closeDevice();

    try {
        m_midiIn->openPort(deviceIndex);
        m_midiIn->setCallback(&MidiInputManager::midiCallback, this);
        // receive sysex (for MIDI Show Control); still ignore timing + active sensing
        m_midiIn->ignoreTypes(false, true, true);

        m_currentDeviceName = QString::fromStdString(m_midiIn->getPortName(deviceIndex));
        m_savedDeviceName = m_currentDeviceName;
        m_deviceOpen = true;

        emit deviceOpened(m_currentDeviceName);
        return true;
    } catch (RtMidiError& error) {
        QString errorMsg = QString::fromStdString(error.getMessage());
        qWarning() << "Error opening MIDI device:" << errorMsg;
        emit deviceError(errorMsg);
        return false;
    }
}

bool MidiInputManager::openDevice(const QString& deviceName) {
    const int idx = findDeviceIndex(deviceName);
    return idx >= 0 ? openDevice(idx) : false;
}

void MidiInputManager::closeDevice() {
    if (!m_midiIn || !m_deviceOpen)
        return;

    try {
        m_midiIn->cancelCallback();
        m_midiIn->closePort();
    } catch (RtMidiError& error) {
        qWarning() << "Error closing MIDI device:" << QString::fromStdString(error.getMessage());
    }

    m_deviceOpen = false;
    m_currentDeviceName.clear();
    emit deviceClosed();
}

void MidiInputManager::setAutoReconnect(bool enabled) { m_autoReconnect = enabled; }

void MidiInputManager::setEnabled(bool enabled) { m_enabled = enabled; }

void MidiInputManager::setMappings(const QVector<MidiMapping>& mappings) {
    m_mappings = mappings;
    emit mappingsChanged();
}

void MidiInputManager::addMapping(const MidiMapping& mapping) {
    m_mappings.append(mapping);
    emit mappingsChanged();
}

void MidiInputManager::removeMapping(int index) {
    if (index >= 0 && index < m_mappings.size()) {
        m_mappings.removeAt(index);
        emit mappingsChanged();
    }
}

void MidiInputManager::clearMappings() {
    m_mappings.clear();
    emit mappingsChanged();
}

bool MidiInputManager::hasConflict(const MidiTrigger& trigger, int excludeIndex) const {
    for (int i = 0; i < m_mappings.size(); ++i) {
        if (i == excludeIndex)
            continue;
        if (m_mappings[i].trigger == trigger)
            return true;
    }
    return false;
}

void MidiInputManager::startMidiLearn() { m_midiLearnActive = true; }

void MidiInputManager::stopMidiLearn() { m_midiLearnActive = false; }

void MidiInputManager::saveToSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("MidiController");

    settings.setValue("enabled", m_enabled);
    settings.setValue("autoReconnect", m_autoReconnect);
    settings.setValue("deviceName", m_savedDeviceName);

    QJsonArray mappingsArray;
    for (const MidiMapping& mapping : m_mappings) {
        mappingsArray.append(mapping.toJson());
    }
    QJsonDocument doc(mappingsArray);
    settings.setValue("mappings", doc.toJson(QJsonDocument::Compact));

    settings.endGroup();
}

void MidiInputManager::loadFromSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("MidiController");

    m_enabled = settings.value("enabled", true).toBool();
    m_autoReconnect = settings.value("autoReconnect", true).toBool();
    m_savedDeviceName = settings.value("deviceName").toString();

    QString mappingsJson = settings.value("mappings").toString();
    if (!mappingsJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(mappingsJson.toUtf8());
        QJsonArray mappingsArray = doc.array();
        m_mappings.clear();
        for (const QJsonValue& val : mappingsArray) {
            m_mappings.append(MidiMapping::fromJson(val.toObject()));
        }
    }

    settings.endGroup();

    // try to open saved device
    if (!m_savedDeviceName.isEmpty() && m_enabled) {
        openDevice(m_savedDeviceName);
    }

    emit mappingsChanged();
}

void MidiInputManager::onDevicePollTimer() {
    if (!m_midiIn)
        return;

    // build current device list
    QStringList currentList;
    QVector<MidiDeviceInfo> devices = availableDevices();
    for (const MidiDeviceInfo& dev : devices) {
        currentList.append(dev.name);
    }

    // check if list changed
    if (currentList != m_lastDeviceList) {
        m_lastDeviceList = currentList;
        emit deviceListChanged();

        // try auto-reconnect if device was disconnected & is now available
        if (m_autoReconnect && !m_deviceOpen && !m_savedDeviceName.isEmpty()) {
            tryReconnect();
        }
    }
}

bool MidiInputManager::tryReconnect() {
    if (m_savedDeviceName.isEmpty())
        return false;
    const int idx = findDeviceIndex(m_savedDeviceName);
    return idx >= 0 ? openDevice(idx) : false;
}

int MidiInputManager::findDeviceIndex(const QString& deviceName) const {
    const auto devices = availableDevices();
    auto it = std::find_if(devices.cbegin(), devices.cend(),
                           [&deviceName](const MidiDeviceInfo& dev) { return dev.name == deviceName; });
    return it != devices.cend() ? it->index : -1;
}

void MidiInputManager::midiCallback(double /*timeStamp*/, std::vector<unsigned char>* message,
                                    void* userData) {
    if (!message || message->empty())
        return;

    MidiInputManager* self = static_cast<MidiInputManager*>(userData);

    // copy message for thread-safe delivery
    std::vector<unsigned char> msgCopy = *message;

    // route to main thread
    QMetaObject::invokeMethod(
        self, [self, msgCopy]() { self->processMidiMessage(msgCopy); }, Qt::QueuedConnection);
}

void MidiInputManager::processMidiMessage(const std::vector<unsigned char>& message) {
    if (message.size() < 2)
        return;

    // System Exclusive — MIDI Show Control frames drive the playback engine
    if (message[0] == 0xF0) {
        handleSysex(message);
        return;
    }

    unsigned char status = message[0];
    int channel = status & 0x0F;
    int statusType = status & 0xF0;

    MidiMessageType type;
    int noteOrCC = message[1];
    int value = (message.size() > 2) ? message[2] : 0;

    switch (statusType) {
    case 0x90: // note on
        // note on with velocity 0 is treated as note off
        if (value == 0) {
            type = MidiMessageType::NoteOff;
        } else {
            type = MidiMessageType::NoteOn;
        }
        break;
    case 0x80:
        type = MidiMessageType::NoteOff;
        break;
    case 0xB0:
        type = MidiMessageType::ControlChange;
        break;
    default:
        return; // ignore other message types
    }

    emit midiMessageReceived(type, channel, noteOrCC, value);

    // MIDI Learn mode
    if (m_midiLearnActive) {
        if ((type == MidiMessageType::NoteOn && value > 0) ||
            (type == MidiMessageType::ControlChange && value > 0)) {
            MidiTrigger trigger;
            trigger.type = type;
            trigger.channel = channel;
            trigger.noteOrCC = noteOrCC;
            trigger.minValue = 1;
            emit midiLearnReceived(trigger);
            m_midiLearnActive = false;
        }
        return;
    }

    // check mappings & execute actions
    if (!m_enabled)
        return;

    for (const MidiMapping& mapping : m_mappings) {
        if (mapping.trigger.matches(type, channel, noteOrCC, value)) {
            executeAction(mapping.action);
            break; // only execute first matching mapping
        }
    }
}

void MidiInputManager::handleSysex(const std::vector<unsigned char>& message) {
    if (!m_enabled || !m_mscEnabled || !m_engine)
        return;

    const MscMessage msc = parseMsc(message);
    if (!msc.valid)
        return;

    // device-id filter: 0x7F = "all devices"
    if (m_mscDeviceId != 0x7F && msc.deviceId != 0x7F && msc.deviceId != m_mscDeviceId)
        return;

    switch (msc.command) {
    case Msc::GO:
    case Msc::TIMED_GO: // timed-go is fired immediately (no fade-from-MSC support yet)
        if (msc.cueNumber.has_value())
            m_engine->goToNumber(*msc.cueNumber);
        m_engine->go();
        break;
    case Msc::STOP:
    case Msc::ALL_OFF:
        m_engine->stop();
        break;
    case Msc::RESUME:
        m_engine->go();
        break;
    default:
        break;
    }

    emit mscReceived(msc.command);
}

void MidiInputManager::executeAction(MidiAction action) {
    if (action == MidiAction::None)
        return;

    emit actionExecuted(action);

    switch (action) {
    case MidiAction::Go:
        if (m_engine)
            m_engine->go();
        break;
    case MidiAction::Stop:
        if (m_engine)
            m_engine->stop();
        break;
    case MidiAction::Pause:
        if (m_engine)
            m_engine->stop();
        break;
    case MidiAction::Resume:
        if (m_engine)
            m_engine->go();
        break;
    case MidiAction::Previous:
        if (m_engine)
            m_engine->previous();
        break;
    case MidiAction::Next:
        if (m_engine)
            m_engine->next();
        break;
    case MidiAction::GoToFirst:
        if (m_engine)
            m_engine->goToFirst();
        break;
    case MidiAction::GoToLast:
        if (m_engine)
            m_engine->goToLast();
        break;
    case MidiAction::Panic:
        if (m_guard)
            m_guard->panic();
        break;
    case MidiAction::PanicImmediate:
        if (m_guard)
            m_guard->panic();
        break;
    case MidiAction::PanicAndRestore:
        if (m_guard)
            m_guard->panic();
        break;
    case MidiAction::RestoreFromPanic:
        break;
    case MidiAction::None:
        break;
    }
}

} // namespace OpenMix
