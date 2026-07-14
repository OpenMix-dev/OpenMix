#include "AvantisProtocol.h"

namespace OpenMix {

AvantisProtocol::AvantisProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void AvantisProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // ACE controls channel level/mute and DCA mute (DCA fader is receive-only)
    for (int i = 1; i <= m_capabilities.inputChannels; ++i) {
        m_snapshotParams.append(QString("/ch/%1/fader").arg(i));
        m_snapshotParams.append(QString("/ch/%1/mute").arg(i));
    }
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 16; ++i) {
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

} // namespace OpenMix
