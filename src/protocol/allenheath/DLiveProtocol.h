#pragma once

#include "AllenHeathTcpProtocol.h"

namespace OpenMix {

// Allen & Heath dLive series (DM0-64, CDM32-64) protocol
// uses binary TCP protocol (ACE) on port 51321
// 16 DCAs, 128 input channels, 64 mix buses
class DLiveProtocol : public AllenHeathTcpProtocol {
    Q_OBJECT

  public:
    explicit DLiveProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath dLive TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;

    // ACE opcodes / plane offsets recovered from DLiveDriver (primary, non-spill
    // path). The channel op extends by 8 (0x30->0x38, 0x31->0x39) on non-1.x
    // software. Bank/spill addressing (op 0x12) applies on build > 90984 when the
    // console reports per-bank handles instead of a global Input Mixer handle
    // (see buildChannelLevelSpill); OpenMix uses the global handle when present.
    quint8 channelLevelOp() const override { return dliveExtended() ? 0x38 : 0x30; }
    quint8 channelMuteOp() const override { return dliveExtended() ? 0x39 : 0x31; }
    int channelMutePlane() const override { return 0x80; }
    int dcaMutePlane() const override { return 0x20; }

  private:
    bool dliveExtended() const {
        return !m_firmwareVersion.isEmpty() && !m_firmwareVersion.startsWith("1.");
    }
};

} // namespace OpenMix
