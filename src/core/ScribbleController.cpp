#include "ScribbleController.h"

#include "Actor.h"
#include "ActorProfileLibrary.h"
#include "Cue.h"
#include "CueList.h"
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
            m_mixer->setChannelName(ch, actor->name());
    }
}

void ScribbleController::onChannelStateChanged(int channel, int state) {
    if (!m_enabled || !m_mixer || channel <= 0)
        return;

    int idx = state;
    if (idx < 0 || idx >= 3)
        idx = static_cast<int>(ChannelState::Normal);
    m_mixer->setChannelColor(channel, m_stateColors[idx]);
}

void ScribbleController::onCurrentCueChanged(int cueIndex) {
    if (!m_enabled || !m_mixer || m_cueChannel <= 0)
        return;

    QString label = QStringLiteral("--");
    if (m_cueList && cueIndex >= 0 && cueIndex < m_cueList->count())
        label = cueLabel(m_cueList->at(cueIndex).number());

    m_mixer->setChannelName(m_cueChannel, label);
}

void ScribbleController::onActorLibraryChanged() { refreshNames(); }

} // namespace OpenMix
