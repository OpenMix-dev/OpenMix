#include "GLDProtocol.h"

namespace OpenMix {

GLDProtocol::GLDProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathMidiProtocol(caps, parent) {
    initializeSnapshotParams();
}

void GLDProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // GLD has 8 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(dcaFaderPath(i));
        m_snapshotParams.append(dcaMutePath(i));
    }
}

QString GLDProtocol::dcaFaderPath(int dca) const { return QString("/dca/%1/fader").arg(dca); }

QString GLDProtocol::dcaMutePath(int dca) const { return QString("/dca/%1/mute").arg(dca); }

} // namespace OpenMix
