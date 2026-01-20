#pragma once

#include "YamahaTcpProtocol.h"

namespace OpenMix {

// Yamaha CL series (CL1, CL3, CL5) protocol
// uses custom TCP protocol on port 49280
// 16 DCAs, 48-72 input channels (model dependent), 24 mix buses
class YamahaCLProtocol : public YamahaTcpProtocol {
    Q_OBJECT

  public:
    explicit YamahaCLProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Yamaha CL TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;
};

} // namespace OpenMix
