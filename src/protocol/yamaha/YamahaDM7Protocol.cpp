#include "YamahaDM7Protocol.h"

namespace OpenMix {

YamahaDM7Protocol::YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaDM7Protocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // DM7 has 16 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 16; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

} // namespace OpenMix
