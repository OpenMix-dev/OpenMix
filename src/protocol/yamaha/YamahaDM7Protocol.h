#pragma once

#include "YamahaProtocol.h"

namespace OpenMix {

// Yamaha DM7 / DM7 Compact.  SCP over TCP 49280.
// 24 DCAs, 120 input channels, 48 mix buses, 24 matrix.
class YamahaDM7Protocol : public YamahaProtocol {
    Q_OBJECT

  public:
    explicit YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    [[nodiscard]] QString protocolDescription() const override { return "Yamaha DM7 SCP Protocol"; }

  protected:
    // DM7 transmits head-amp gain in whole dB (-6..+66), unlike the centi-dB
    // CL/QL/Rivage use.
    [[nodiscard]] int preampGainToScp(double gainDb) const override;
};

} // namespace OpenMix
