#pragma once

#include "YamahaTcpProtocol.h"

namespace OpenMix {

// Yamaha TF series (TF1, TF3, TF5) protocol
// uses SCP over TCP on port 49280
// 4 DCAs, 16-40 input channels (model dependent), 20 mix buses
class YamahaTFProtocol : public YamahaTcpProtocol {
    Q_OBJECT

  public:
    explicit YamahaTFProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Yamaha TF SCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;

    // TF uses slightly different command format
    QString buildDCAFaderCommand(int dca, float level) override;
    QString buildDCAMuteCommand(int dca, bool muted) override;
    QString buildSceneRecallCommand(int sceneNumber) override;
};

} // namespace OpenMix
