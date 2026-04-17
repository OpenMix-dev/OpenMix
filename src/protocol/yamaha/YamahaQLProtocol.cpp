#include "YamahaQLProtocol.h"

namespace OpenMix {

YamahaQLProtocol::YamahaQLProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaQLProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // QL series input channels (up to 64), 16 effect-send buses max
    appendEqSnapshotParams(m_snapshotParams, "/ch/", 64, 2, 16);

    // QL series has 8 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

} // namespace OpenMix
