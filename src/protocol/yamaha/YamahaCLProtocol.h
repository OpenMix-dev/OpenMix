#pragma once

#include "YamahaProtocol.h"

namespace OpenMix {

// Yamaha CL series (CL1, CL3, CL5) protocol
// 16 DCAs, 48-72 input channels (model dependent), 24 mix buses, 8 matrix
class YamahaCLProtocol : public YamahaProtocol {
    Q_OBJECT

  public:
    explicit YamahaCLProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    [[nodiscard]] QString protocolDescription() const override { return "Yamaha CL Protocol"; }

  protected:
    void initializeSnapshotParams() override;
};

} // namespace OpenMix
