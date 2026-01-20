#include "MixerProtocol.h"

namespace OpenMix {

// subclasses should override to return their specific capabilities
const MixerCapabilities& MixerProtocol::capabilities() const {
    static MixerCapabilities defaultCaps = MixerCapabilities::forConsole(ConsoleType::Unknown);
    return defaultCaps;
}

} // namespace OpenMix
