#include "YamahaTFProtocol.h"

namespace OpenMix {

YamahaTFProtocol::YamahaTFProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaTFProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // input channels w/ EQ
    for (int ch = 1; ch <= m_capabilities.inputChannels && ch <= 32; ++ch) {
        QString chPrefix = QString("/ch/%1").arg(ch, 2, 10, QChar('0'));
        m_snapshotParams.append(chPrefix + "/mix/fader");
        m_snapshotParams.append(chPrefix + "/mix/on");

        // EQ parameters
        if (m_capabilities.supportsChannelEQ) {
            m_snapshotParams.append(chPrefix + "/eq/on");
            for (int band = 1; band <= m_capabilities.eqBandsPerChannel; ++band) {
                QString bandPrefix = QString("%1/eq/%2").arg(chPrefix).arg(band);
                m_snapshotParams.append(bandPrefix + "/type");
                m_snapshotParams.append(bandPrefix + "/f");
                m_snapshotParams.append(bandPrefix + "/g");
                m_snapshotParams.append(bandPrefix + "/q");
            }
        }

        // effect send parameters (TF has 8)
        if (m_capabilities.supportsEffectSends) {
            int sends = qMin(m_capabilities.effectSendBuses, 8);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_snapshotParams.append(sendPrefix + "/level");
                m_snapshotParams.append(sendPrefix + "/on");
            }
        }
    }

    // DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 4; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

QString YamahaTFProtocol::buildDCAFaderCommand(int dca, float level) {
    int levelInt = qBound(0, static_cast<int>(level * 1000.0f), 1000);
    return QString("set MIXER:Current/DcaCh/Fader/Level %1 0 %2").arg(dca - 1).arg(levelInt);
}

QString YamahaTFProtocol::buildDCAMuteCommand(int dca, bool muted) {
    return QString("set MIXER:Current/DcaCh/Mute/On %1 0 %2").arg(dca - 1).arg(muted ? 1 : 0);
}

QString YamahaTFProtocol::buildSceneRecallCommand(int sceneNumber) {
    return QString("ssrecall_ex MIXER:Current/SceneMem/CurrentSlot %1").arg(sceneNumber);
}

} // namespace OpenMix
