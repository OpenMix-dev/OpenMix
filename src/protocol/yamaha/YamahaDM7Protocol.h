#pragma once

#include "YamahaProtocol.h"

namespace OpenMix {

// Yamaha DM7 / DM7 Compact protocol
// 24 DCAs, 120 input channels, 48 mix buses, 24 matrix
class YamahaDM7Protocol : public YamahaProtocol {
    Q_OBJECT

  public:
    explicit YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    [[nodiscard]] QString protocolDescription() const override { return "Yamaha DM7 Protocol"; }

  protected:
    void initializeSnapshotParams() override;
};

} // namespace OpenMix
