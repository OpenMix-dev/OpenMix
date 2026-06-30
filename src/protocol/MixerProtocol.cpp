#include "MixerProtocol.h"

namespace OpenMix {

// subclasses should override to return their specific capabilities
const MixerCapabilities& MixerProtocol::capabilities() const {
    static MixerCapabilities defaultCaps = MixerCapabilities::forConsole(ConsoleType::Unknown);
    return defaultCaps;
}

// default no-op implementations of the semantic channel setters; drivers that
// support direct channel control override the ones they can encode.
void MixerProtocol::setChannelFader(int, double) {}
void MixerProtocol::setChannelMute(int, bool) {}
void MixerProtocol::setChannelPreamp(int, double) {}
void MixerProtocol::setChannelHpf(int, bool, double) {}
void MixerProtocol::setChannelEqOn(int, bool) {}
void MixerProtocol::setChannelEqBand(int, int, bool, int, double, double, double) {}
void MixerProtocol::setChannelDynamics(int, bool, double, double, double, double, double) {}

} // namespace OpenMix
