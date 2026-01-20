#include "SQProtocol.h"

namespace OpenMix {

SQProtocol::SQProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathMidiProtocol(caps, parent) {
    initializeSnapshotParams();
}

void SQProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(dcaFaderPath(i));
        m_snapshotParams.append(dcaMutePath(i));
    }
}

QString SQProtocol::dcaFaderPath(int dca) const { return QString("/dca/%1/fader").arg(dca); }

QString SQProtocol::dcaMutePath(int dca) const { return QString("/dca/%1/mute").arg(dca); }

} // namespace OpenMix
