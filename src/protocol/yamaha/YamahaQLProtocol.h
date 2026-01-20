#pragma once

#include "YamahaTcpProtocol.h"

namespace OpenMix {

// Yamaha QL series (QL1, QL5) protocol
// uses custom TCP protocol on port 49280
// 8 DCAs, 32-64 input channels (model dependent), 16 mix buses
class YamahaQLProtocol : public YamahaTcpProtocol {
    Q_OBJECT

  public:
    explicit YamahaQLProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Yamaha QL TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;
};

} // namespace OpenMix
