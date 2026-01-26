#pragma once

#include "../MixerCapabilities.h"
#include <QHostAddress>
#include <QString>

namespace OpenMix {

struct DiscoveredConsole {
    QHostAddress address;
    int port = 0;
    Manufacturer manufacturer = Manufacturer::Unknown;
    ConsoleType type = ConsoleType::Unknown;
    QString modelName;
    QString displayName;
    QString firmwareVersion;

    bool isValid() const { return !address.isNull() && type != ConsoleType::Unknown; }

    MixerCapabilities toCapabilities() const;

    QString toString() const;

    bool operator==(const DiscoveredConsole& other) const {
        return address == other.address && port == other.port && type == other.type;
    }
};

} // namespace OpenMix
