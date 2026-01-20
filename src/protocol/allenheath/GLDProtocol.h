#pragma once

#include "AllenHeathMidiProtocol.h"

namespace OpenMix {

// Allen & Heath GLD series (GLD-80, GLD-112) protocol
// uses MIDI over TCP on port 51325
// 8 DCAs, up to 48 input channels, 20-30 mix buses
class GLDProtocol : public AllenHeathMidiProtocol {
    Q_OBJECT

  public:
    explicit GLDProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath GLD MIDI/TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;
    QString dcaFaderPath(int dca) const override;
    QString dcaMutePath(int dca) const override;
};

} // namespace OpenMix
