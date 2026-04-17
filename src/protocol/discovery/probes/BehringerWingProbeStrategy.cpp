#include "BehringerWingProbeStrategy.h"

namespace OpenMix {

DiscoveredConsole BehringerWingProbeStrategy::parseResponse([[maybe_unused]] const QString& path,
                                                            const QVariant& value,
                                                            const QHostAddress& sender,
                                                            [[maybe_unused]] int senderPort) {

    DiscoveredConsole console;
    console.address = sender;
    console.port = 2223;

    QString modelString = value.toString();
    console.modelName = modelString;
    console.type = identifyModel(modelString);

    if (console.type != ConsoleType::Unknown) {
        MixerCapabilities caps = MixerCapabilities::forConsole(console.type);
        console.manufacturer = caps.manufacturer;
        console.displayName = caps.displayName;
    } else {
        // if we got a response on port 2223 to /$info, it's likely a WING
        console.type = ConsoleType::Wing;
        console.manufacturer = Manufacturer::Behringer;
        console.displayName = "Behringer WING";
    }

    return console;
}

ConsoleType BehringerWingProbeStrategy::identifyModel(const QString& modelString) const {
    QString model = modelString.toLower();

    if (model.contains("wing compact") || model.contains("wingcompact")) {
        return ConsoleType::Wing; // WING Compact uses same protocol
    }

    if (model.contains("wing")) {
        return ConsoleType::Wing;
    }

    return ConsoleType::Unknown;
}

} // namespace OpenMix
