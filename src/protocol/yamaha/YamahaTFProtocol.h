#pragma once

#include "YamahaProtocol.h"

namespace OpenMix {

// Yamaha TF series (TF1, TF3, TF5) protocol
// 8 DCAs, 16-40 input channels (model dependent), 20 mix buses
class YamahaTFProtocol : public YamahaProtocol {
    Q_OBJECT

  public:
    explicit YamahaTFProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    [[nodiscard]] QString protocolDescription() const override { return "Yamaha TF Protocol"; }

  protected:
    void initializeSnapshotParams() override;
};

} // namespace OpenMix
