#pragma once

#include "YamahaProtocol.h"

namespace OpenMix {

// Yamaha TF series (TF1, TF3, TF5).
// NOTE: the TF series uses a more limited control protocol than the CL/QL/
// Rivage/DM7 SCP it shares a base with here.  This driver is a best-effort
// SCP stub for TF; correctness is targeted at CL/QL/DM7.
class YamahaTFProtocol : public YamahaProtocol {
    Q_OBJECT

  public:
    explicit YamahaTFProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    [[nodiscard]] QString protocolDescription() const override { return "Yamaha TF SCP Protocol"; }
};

} // namespace OpenMix
