#include "YamahaTFProtocol.h"

namespace OpenMix {

YamahaTFProtocol::YamahaTFProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaTFProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    for (int i = 1; i <= m_capabilities.dcaCount && i <= 4; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

QString YamahaTFProtocol::buildDCAFaderCommand(int dca, float level) {
    // TF series SCP command format
    // MIXER:Current/DcaCh/Fader/Level [dca] [0] [value]
    int levelInt = qBound(0, static_cast<int>(level * 1000.0f), 1000);
    return QString("set MIXER:Current/DcaCh/Fader/Level %1 0 %2").arg(dca - 1).arg(levelInt);
}

QString YamahaTFProtocol::buildDCAMuteCommand(int dca, bool muted) {
    // TF series SCP command format
    // MIXER:Current/DcaCh/Mute/On [dca] [0] [value]
    return QString("set MIXER:Current/DcaCh/Mute/On %1 0 %2").arg(dca - 1).arg(muted ? 1 : 0);
}

QString YamahaTFProtocol::buildSceneRecallCommand(int sceneNumber) {
    // TF series scene recall
    return QString("ssrecall_ex MIXER:Current/SceneMem/CurrentSlot %1").arg(sceneNumber);
}

} // namespace OpenMix
