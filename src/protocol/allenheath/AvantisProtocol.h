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
};

} // namespace OpenMix
