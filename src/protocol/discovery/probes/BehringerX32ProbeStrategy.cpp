#include "BehringerX32ProbeStrategy.h"

namespace OpenMix {

DiscoveredConsole BehringerX32ProbeStrategy::parseResponse([[maybe_unused]] const QString& path,
                                                           const QVariantList& args,
                                                           const QHostAddress& sender,
                                                           [[maybe_unused]] int senderPort) {

    DiscoveredConsole console;
    console.address = sender;
    console.port = 10023;

    // /xinfo reply: [ip, name, model, firmware]
    QString modelString = args.value(2).toString();
    if (modelString.isEmpty()) {
        // unexpected arg layout: fall back to the first arg that names a model
        for (const QVariant& arg : args) {
            const QString candidate = arg.toString().toLower();
            if (candidate.contains("x32") || candidate.contains("m32")) {
                modelString = arg.toString();
                break;
            }
        }
    }

    if (modelString.isEmpty()) {
        return console;
    }

    console.modelName = modelString;
    console.firmwareVersion = args.value(3).toString();
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
