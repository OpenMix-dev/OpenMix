#include "SpareBackup.h"

namespace OpenMix {

SpareBackup::SpareBackup(QObject* parent) : QObject(parent) {}

void SpareBackup::setSpareChannel(int channel) {
    if (m_spareChannel == channel)
        return;
    m_spareChannel = channel;
    // dropping the spare invalidates any allocation/switch
    if (m_spareChannel < 0) {
        m_allocatedChannel = -1;
        setState(State::Inactive);
        emit allocationChanged(m_allocatedChannel);
    }
    emit changed();
}

bool SpareBackup::allocateTo(int channel) {
    if (m_spareChannel < 0 || channel < 0)
        return false;
    if (m_state == State::Active) // can't reallocate a live spare
        return false;
    if (m_allocatedChannel == channel)
        return true;
    m_allocatedChannel = channel;
    setState(State::Inactive);
    emit allocationChanged(m_allocatedChannel);
    emit changed();
    return true;
}

void SpareBackup::removeAllocation() {
    if (m_allocatedChannel < 0 && m_state == State::Inactive)
        return;
    m_allocatedChannel = -1;
    setState(State::Inactive);
    emit allocationChanged(m_allocatedChannel);
    emit changed();
}

void SpareBackup::switchNow() {
    if (!isAllocated())
        return;
    setState(State::Active);
    emit changed();
}

void SpareBackup::switchLater() {
    if (!isAllocated())
        return;
    setState(State::Armed);
    emit changed();
}

void SpareBackup::revert() {
    if (m_state == State::Inactive)
        return;
    setState(State::Inactive);
    emit changed();
}

void SpareBackup::onCueFired() {
    if (m_state == State::Armed)
        setState(State::Active);
}

void SpareBackup::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

QJsonObject SpareBackup::toJson() const {
    QJsonObject json;
    json["spareChannel"] = m_spareChannel;
    json["allocatedChannel"] = m_allocatedChannel;
    json["state"] = static_cast<int>(m_state);
    return json;
}

void SpareBackup::loadFromJson(const QJsonObject& json) {
    m_spareChannel = json.value("spareChannel").toInt(-1);
    m_allocatedChannel = json.value("allocatedChannel").toInt(-1);
    const int stateInt = json.value("state").toInt(0);
    m_state = (stateInt >= static_cast<int>(State::Inactive) &&
               stateInt <= static_cast<int>(State::Active))
                  ? static_cast<State>(stateInt)
                  : State::Inactive;
    if (m_spareChannel < 0) {
        m_allocatedChannel = -1;
        m_state = State::Inactive;
    }
    emit allocationChanged(m_allocatedChannel);
    emit stateChanged(m_state);
    emit changed();
}

} // namespace OpenMix
