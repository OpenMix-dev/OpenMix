#pragma once

#include "YamahaProtocol.h"

namespace OpenMix {

// Yamaha QL series (QL1, QL5).  SCP over TCP 49280.
// 8 DCAs, 32-64 input channels (model dependent), 16 mix buses, 8 matrix.
class YamahaQLProtocol : public YamahaProtocol {
    Q_OBJECT

  public:
    explicit YamahaQLProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    [[nodiscard]] QString protocolDescription() const override { return "Yamaha QL SCP Protocol"; }
};

} // namespace OpenMix
