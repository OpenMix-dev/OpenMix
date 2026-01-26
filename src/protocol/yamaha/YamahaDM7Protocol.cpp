#include "YamahaDM7Protocol.h"

namespace OpenMix {

YamahaDM7Protocol::YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaProtocol(caps, parent) {
    initializeSnapshotParams();
}

void YamahaDM7Protocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // DM7 input channels (up to 120)
    for (int ch = 1; ch <= m_capabilities.inputChannels && ch <= 120; ++ch) {
        QString chPrefix = QString("/ch/%1").arg(ch, 3, 10, QChar('0'));
        m_snapshotParams.append(chPrefix + "/mix/fader");
        m_snapshotParams.append(chPrefix + "/mix/on");

        // EQ parameters - DM7 has 6-band EQ
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
            int sends = qMin(m_capabilities.effectSendBuses, 24);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_snapshotParams.append(sendPrefix + "/level");
                m_snapshotParams.append(sendPrefix + "/on");
            }
        }
    }

    // DM7 has 24 DCAs
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 24; ++i) {
        m_snapshotParams.append(QString("/dca/%1/fader").arg(i));
        m_snapshotParams.append(QString("/dca/%1/on").arg(i));
    }
}

} // namespace OpenMix
