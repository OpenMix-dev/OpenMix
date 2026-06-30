#include "ChannelMonitor.h"
#include <QTimer>

namespace OpenMix {

ChannelMonitor::ChannelMonitor(QObject* parent) : QObject(parent) {}

ChannelMonitor::ChannelData& ChannelMonitor::ensureChannel(int channel) {
    auto it = m_channels.find(channel);
    if (it != m_channels.end()) {
        return *it;
    }

    ChannelData data;
    data.silenceTimer = new QTimer(this);
    data.silenceTimer->setSingleShot(true);
    connect(data.silenceTimer, &QTimer::timeout, this,
            [this, channel]() { setState(channel, ChannelState::Silent); });

    data.clipTimer = new QTimer(this);
    data.clipTimer->setSingleShot(true);
    connect(data.clipTimer, &QTimer::timeout, this,
            [this, channel]() { onClipHoldExpired(channel); });

    return *m_channels.insert(channel, data);
}

void ChannelMonitor::setState(int channel, ChannelState state) {
    auto it = m_channels.find(channel);
    if (it == m_channels.end()) {
        return;
    }
    if (it->state != state) {
        it->state = state;
        emit channelStateChanged(channel, static_cast<int>(state));
    }
}

void ChannelMonitor::onClipHoldExpired(int channel) {
    // clip ceiling no longer held: drop back to Normal; the next level samples
    // re-evaluate (and may re-arm silence detection if the input has gone quiet)
    setState(channel, ChannelState::Normal);
}

void ChannelMonitor::onLevel(int channel, double level) {
    ChannelData& data = ensureChannel(channel);

    // clipping takes priority and (re)starts the hold window
    if (level >= m_clipThreshold) {
        data.silenceTimer->stop();
        setState(channel, ChannelState::Clipping);
        data.clipTimer->start(m_clipHoldMs);
        return;
    }

    // still inside the clip-hold window: keep Clipping until the timer expires
    if (data.clipTimer->isActive()) {
        return;
    }

    if (level <= m_silenceThreshold) {
        if (data.state == ChannelState::Silent) {
            return; // already silent; nothing to do
        }
        // arm the silence timeout; state stays Normal until it elapses
        if (!data.silenceTimer->isActive()) {
            data.silenceTimer->start(m_silenceTimeoutMs);
        }
    } else {
        // audio present in band: cancel any pending silence and return to Normal
        data.silenceTimer->stop();
        setState(channel, ChannelState::Normal);
    }
}

ChannelState ChannelMonitor::channelState(int channel) const {
    auto it = m_channels.constFind(channel);
    return it != m_channels.constEnd() ? it->state : ChannelState::Normal;
}

void ChannelMonitor::reset() {
    for (ChannelData& data : m_channels) {
        if (data.silenceTimer) {
            data.silenceTimer->stop();
            data.silenceTimer->deleteLater();
        }
        if (data.clipTimer) {
            data.clipTimer->stop();
            data.clipTimer->deleteLater();
        }
    }
    m_channels.clear();
}

} // namespace OpenMix
