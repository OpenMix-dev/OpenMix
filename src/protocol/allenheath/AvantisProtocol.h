#pragma once

#include "AllenHeathTcpProtocol.h"

namespace OpenMix {

// Allen & Heath Avantis / Avantis Solo protocol
// uses binary TCP protocol (ACE) on port 51321
// 16 DCAs, 64 input channels, 36 mix buses
class AvantisProtocol : public AllenHeathTcpProtocol {
    Q_OBJECT

  public:
    explicit AvantisProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath Avantis TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;

    // ACE opcodes / plane offsets recovered from AvantisDriver. The channel op
    // extends by 6 (0x25->0x2B, 0x26->0x2C) on firmware build > 96884.
    quint8 channelLevelOp() const override { return m_firmwareRev > 96884 ? 0x2B : 0x25; }
    quint8 channelMuteOp() const override { return m_firmwareRev > 96884 ? 0x2C : 0x26; }
    int channelMutePlane() const override { return 0x20; }
    int dcaMutePlane() const override { return 0x18; }
};

} // namespace OpenMix
