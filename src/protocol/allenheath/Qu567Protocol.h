#pragma once

#include "SQProtocol.h"

namespace OpenMix {

// Allen & Heath Qu-5, Qu-6, Qu-7 (2024), MIDI over TCP 51325, per the Qu-5/6/7
// MIDI Protocol Issue 2. The scheme is SQ's: the parameter number rides the
// NRPN MSB/LSB, levels are 14-bit through the console's NRPN Fader Law, mutes
// are NRPN, scenes are Bank + Program Change in banks of 128. What differs is
// the size of the desk, which the capabilities carry. Not to be confused with
// the Qu-16/24/32 (QuProtocol), whose V1.9 map puts the channel in the MSB.
class Qu567Protocol : public SQProtocol {
    Q_OBJECT

  public:
    explicit Qu567Protocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override {
        return "Allen & Heath Qu-5/6/7 MIDI/TCP Protocol";
    }
};

} // namespace OpenMix
