#include "BehringerX32ProbeStrategy.h"

namespace OpenMix {

DiscoveredConsole BehringerX32ProbeStrategy::parseResponse(const QString& path,
                                                           const QVariant& value,
                                                           const QHostAddress& sender,
                                                           int senderPort) {
    Q_UNUSED(path);
    Q_UNUSED(senderPort);

    DiscoveredConsole console;
    console.address = sender;
    console.port = 10023;

    QString modelString = value.toString();

    if (modelString.isEmpty()) {
        return console;
    }

    console.modelName = modelString;
    console.type = identifyModel(modelString);

    if (console.type != ConsoleType::Unknown) {
        MixerCapabilities caps = MixerCapabilities::forConsole(console.type);
        console.manufacturer = caps.manufacturer;
        console.displayName = caps.displayName;
    }

    return console;
}

ConsoleType BehringerX32ProbeStrategy::identifyModel(const QString& modelString) const {
    QString model = modelString.toLower();

    // Midas M32 variants
    if (model.contains("m32")) {
        return ConsoleType::M32;
    }

    // Behringer X32 variants
    if (model.contains("x32")) {
        return ConsoleType::X32;
    }

    // fallback: if it responds to /xinfo, it's likely X32-compatible
    if (!modelString.isEmpty()) {
        return ConsoleType::X32;
    }

    return ConsoleType::Unknown;
}

} // namespace OpenMix
