#pragma once

#include "AllenHeathMidiProtocol.h"

namespace OpenMix {

// Allen & Heath SQ series (SQ-5, SQ-6, SQ-7) protocol
// uses MIDI over TCP on port 51325
// 8 DCAs, up to 48 input channels, 12 mix buses
class SQProtocol : public AllenHeathMidiProtocol {
    Q_OBJECT

  public:
    explicit SQProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath SQ MIDI/TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;
    QString dcaFaderPath(int dca) const override;
    QString dcaMutePath(int dca) const override;
};

} // namespace OpenMix
