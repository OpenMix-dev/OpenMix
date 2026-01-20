#include "YamahaCLProtocol.h"

namespace OpenMix {

YamahaCLProtocol::YamahaCLProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaCLProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // CL series has 16 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 16; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

} // namespace OpenMix
