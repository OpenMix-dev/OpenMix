#pragma once

#include "AllenHeathMidiProtocol.h"

namespace OpenMix {

// Allen & Heath Qu series (Qu-16, Qu-24, Qu-32) protocol.
// Qu shares the SQ MIDI-over-TCP control path (port 51325); all Qu models run
// the same 32-channel DSP, differing only in physical fader count.
// 8 DCAs, 32 input channels, 12 mix buses.
class QuProtocol : public AllenHeathMidiProtocol {
    Q_OBJECT

  public:
    explicit QuProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath Qu MIDI/TCP Protocol"; }

  protected:
    void initializeSnapshotParams() override;
    QString dcaFaderPath(int dca) const override;
    QString dcaMutePath(int dca) const override;
};

} // namespace OpenMix
