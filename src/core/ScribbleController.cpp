#include "ScribbleController.h"

#include "Actor.h"
#include "ActorProfileLibrary.h"
#include "Cue.h"
#include "CueList.h"
#include "DCAMapping.h"
#include "protocol/MixerProtocol.h"

#include <QSet>

namespace OpenMix {

namespace {
// short strip label for a cue number: "Q1", "Q2.5" (no trailing .0)
QString cueLabel(double number) {
    const bool integral = number == static_cast<double>(static_cast<long long>(number));
    return QStringLiteral("Q") + QString::number(number, 'f', integral ? 0 : 1);
}
} // namespace

ScribbleController::ScribbleController(QObject* parent) : QObject(parent) {}

void ScribbleController::setMixer(MixerProtocol* mixer) {
    if (m_mixer == mixer)
        return;
    if (m_mixer)
        disconnect(m_mixer, nullptr, this, nullptr);

    m_mixer = mixer;

    if (m_mixer) {
        // push names once the console finishes connecting
        connect(m_mixer, &MixerProtocol::connected, this, &ScribbleController::refreshNames);
        if (m_mixer->isConnected())
            refreshNames();
    }
}

void ScribbleController::setActorLibrary(ActorProfileLibrary* library) { m_library = library; }

void ScribbleController::setCueList(CueList* cueList) { m_cueList = cueList; }

void ScribbleController::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (m_enabled)
        refreshNames();
}

void ScribbleController::setHighlightEnabled(bool enabled) {
    if (m_highlightEnabled == enabled)
        return;
    m_highlightEnabled = enabled;

    if (m_highlightEnabled) {
        updateHighlight(m_currentCueIndex);
    } else if (m_mixer) {
        // restore every highlighted strip to the Normal state color
        const int normal = m_stateColors[static_cast<int>(ChannelState::Normal)];
        for (int ch : m_highlightedChannels)
            m_mixer->setChannelColor(ch, normal);
        m_highlightedChannels.clear();
    }
}

void ScribbleController::setCueNumberChannel(int channel) {
    m_cueChannel = channel > 0 ? channel : 0;
}

void ScribbleController::setStateColor(ChannelState state, int color) {
    const int idx = static_cast<int>(state);
    if (idx >= 0 && idx < 3)
        m_stateColors[idx] = color;
}

int ScribbleController::stateColor(ChannelState state) const {
    const int idx = static_cast<int>(state);
    return (idx >= 0 && idx < 3) ? m_stateColors[idx] : m_stateColors[0];
}

void ScribbleController::refreshNames() {
    if (!m_enabled || !m_mixer || !m_library)
        return;

    // resolve one winning actor per channel (lead over understudy) so a channel
    // shared by several cast members shows a single, stable name.
    QSet<int> channels;
    for (const Actor& a : m_library->actors()) {
        if (a.channel() > 0)
            channels.insert(a.channel());
    }

    for (int ch : channels) {
        if (ch == m_cueChannel)
            continue; // reserved for the cue number
        if (const Actor* actor = m_library->actorForChannel(ch))
            m_mixer->setChannelName(ch, actor->displayName());
    }
}

void ScribbleController::onChannelStateChanged(int channel, int state) {
    if (!m_enabled || !m_mixer || channel <= 0)
        return;

    int idx = state;
    if (idx < 0 || idx >= 3)
        idx = static_cast<int>(ChannelState::Normal);

    int color = m_stateColors[idx];
    // a highlighted channel keeps its highlight while it reads Normal; silence
    // and clip warnings still win so the operator sees them.
    if (idx == static_cast<int>(ChannelState::Normal) && m_highlightEnabled &&
        m_highlightedChannels.contains(channel)) {
        color = m_highlightColor;
    }
    m_mixer->setChannelColor(channel, color);
}

void ScribbleController::onCurrentCueChanged(int cueIndex) {
    m_currentCueIndex = cueIndex;

    if (m_enabled && m_mixer && m_cueChannel > 0) {
        QString label = QStringLiteral("--");
        if (m_cueList && cueIndex >= 0 && cueIndex < m_cueList->count())
            label = cueLabel(m_cueList->at(cueIndex).number());
        m_mixer->setChannelName(m_cueChannel, label);
    }

    updateHighlight(cueIndex);
}

void ScribbleController::onActorLibraryChanged() { refreshNames(); }

QSet<int> ScribbleController::channelsTouchedByCue(const Cue& cue) const {
    QSet<int> channels;

    for (int ch : cue.channelProfiles().keys())
        if (ch > 0)
            channels.insert(ch);
    for (int ch : cue.channelLevels().keys())
        if (ch > 0)
            channels.insert(ch);

    // channels reached through the cue's targeted DCAs (per-cue custom mapping
    // if present, otherwise the show-level mapping)
    const QSet<int> targetDCAs = cue.targetedDCAs(); // empty = all DCAs
    if (cue.hasCustomDCAMapping()) {
        const QMap<int, QList<int>> mapping = cue.dcaChannelMapping();
        for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
            if (!targetDCAs.isEmpty() && !targetDCAs.contains(it.key()))
                continue;
            for (int ch : it.value())
                if (ch > 0)
                    channels.insert(ch);
        }
    } else if (m_dcaMapping) {
        const QSet<int> dcas = targetDCAs.isEmpty() ? m_dcaMapping->assignedDCAs() : targetDCAs;
        for (int dca : dcas)
            for (int ch : m_dcaMapping->channelsForDCA(dca))
                if (ch > 0)
                    channels.insert(ch);
    }

    return channels;
}

void ScribbleController::updateHighlight(int cueIndex) {
    if (!m_highlightEnabled || !m_enabled || !m_mixer)
        return;

    QSet<int> touched;
    if (m_cueList && cueIndex >= 0 && cueIndex < m_cueList->count())
        touched = channelsTouchedByCue(m_cueList->at(cueIndex));

    // restore strips that are no longer touched to the Normal state color
    const int normal = m_stateColors[static_cast<int>(ChannelState::Normal)];
    for (int ch : m_highlightedChannels) {
        if (!touched.contains(ch))
            m_mixer->setChannelColor(ch, normal);
    }

    for (int ch : touched)
        m_mixer->setChannelColor(ch, m_highlightColor);

    m_highlightedChannels = touched;
}

} // namespace OpenMix
