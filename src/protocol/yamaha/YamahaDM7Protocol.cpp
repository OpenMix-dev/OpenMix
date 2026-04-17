#include "YamahaDM7Protocol.h"

namespace OpenMix {

YamahaDM7Protocol::YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaDM7Protocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // DM7 input channels (up to 120), 3-digit channel field, 24 effect-send buses max
    appendEqSnapshotParams(m_snapshotParams, "/ch/", 120, 3, 24);

    // DM7 has 24 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 24; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

} // namespace OpenMix
