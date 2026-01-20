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
};

} // namespace OpenMix
