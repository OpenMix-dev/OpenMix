#pragma once

#include "YamahaTcpProtocol.h"

namespace OpenMix {

// Yamaha DM7 / DM7 Compact protocol
// uses custom TCP protocol on port 49280
// 16 DCAs, 120 input channels, 48 mix buses
class YamahaDM7Protocol : public YamahaTcpProtocol {
    Q_OBJECT

  public:
    explicit YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Yamaha DM7 TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;
};

} // namespace OpenMix
