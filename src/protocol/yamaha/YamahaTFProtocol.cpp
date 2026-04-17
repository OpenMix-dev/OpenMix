#include "YamahaTFProtocol.h"

namespace OpenMix {

YamahaTFProtocol::YamahaTFProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaTFProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // TF series input channels (up to 40), 8 effect-send buses max
    appendEqSnapshotParams(m_snapshotParams, "/ch/", 40, 2, 8);

    // TF series has 8 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

} // namespace OpenMix
