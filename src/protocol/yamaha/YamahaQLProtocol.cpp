#include "YamahaQLProtocol.h"

namespace OpenMix {

YamahaQLProtocol::YamahaQLProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaQLProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // QL series has 8 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

} // namespace OpenMix
