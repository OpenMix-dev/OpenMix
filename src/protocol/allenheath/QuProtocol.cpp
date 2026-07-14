#include "QuProtocol.h"

namespace OpenMix {

QuProtocol::QuProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathMidiProtocol(caps, parent) {
    initializeSnapshotParams();
}

void QuProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(dcaFaderPath(i));
        m_snapshotParams.append(dcaMutePath(i));
    }

    // input-channel faders, so a cue restores channel levels (not just DCAs)
    for (int i = 1; i <= m_capabilities.inputChannels; ++i) {
        m_snapshotParams.append(QString("/ch/%1/fader").arg(i));
    }
}

QString QuProtocol::dcaFaderPath(int dca) const { return QString("/dca/%1/fader").arg(dca); }

QString QuProtocol::dcaMutePath(int dca) const { return QString("/dca/%1/mute").arg(dca); }

} // namespace OpenMix
