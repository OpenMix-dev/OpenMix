#include "YamahaCLProtocol.h"

namespace OpenMix {

YamahaCLProtocol::YamahaCLProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaCLProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // CL series input channels (up to 72), 16 effect-send buses max
    appendEqSnapshotParams(m_snapshotParams, "/ch/", 72, 2, 16);

    // CL series has 16 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 16; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

} // namespace OpenMix
