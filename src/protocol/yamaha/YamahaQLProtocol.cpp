#include "YamahaQLProtocol.h"

namespace OpenMix {

YamahaQLProtocol::YamahaQLProtocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaTcpProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaQLProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // input channels w/ EQ (QL series has up to 64)
    for (int ch = 1; ch <= m_capabilities.inputChannels && ch <= 64; ++ch) {
        QString chPrefix = QString("/ch/%1").arg(ch, 2, 10, QChar('0'));
        m_snapshotParams.append(chPrefix + "/mix/fader");
        m_snapshotParams.append(chPrefix + "/mix/on");

        // EQ parameters (4-band PEQ)
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

        // effect send parameters
        if (m_capabilities.supportsEffectSends) {
            int sends = qMin(m_capabilities.effectSendBuses, 16);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_snapshotParams.append(sendPrefix + "/level");
                m_snapshotParams.append(sendPrefix + "/on");
            }
        }
    }

    // QL series has 8 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

} // namespace OpenMix
