#include "DiscoveredConsole.h"

namespace OpenMix {

MixerCapabilities DiscoveredConsole::toCapabilities() const {
    MixerCapabilities caps = MixerCapabilities::forConsole(type);

    if (port > 0) {
        caps.defaultPort = port;
    }

    if (!displayName.isEmpty()) {
        caps.displayName = displayName;
    }

    return caps;
}

QString DiscoveredConsole::toString() const {
    QString result = displayName.isEmpty() ? modelName : displayName;

    if (result.isEmpty()) {
        result = "Unknown Console";
    }

    result += QString(" at %1").arg(address.toString());

    if (port > 0) {
        result += QString(":%1").arg(port);
    }

    return result;
}

} // namespace OpenMix
