#include "BehringerWingProbeStrategy.h"

namespace OpenMix {

DiscoveredConsole BehringerWingProbeStrategy::parseResponse(
    [[maybe_unused]] const QString& path, [[maybe_unused]] const QVariantList& args,
    [[maybe_unused]] const QHostAddress& sender, [[maybe_unused]] int senderPort) {
    // WING discovery is raw-datagram only
    return {};
}

DiscoveredConsole BehringerWingProbeStrategy::parseRawResponse(const QByteArray& data,
                                                               const QHostAddress& sender,
                                                               [[maybe_unused]] int senderPort) {
    DiscoveredConsole console;

    // "WING,<ip>,<name>,<model>,<serial>,<firmware>"
    const QList<QByteArray> fields = data.split(',');
    if (fields.size() < 6) {
        return console;
    }

    console.address = sender;
    console.port = 2223; // OSC control port
    console.modelName = QString::fromUtf8(fields[3]).trimmed();
    console.firmwareVersion = QString::fromUtf8(fields[5]).trimmed();
    console.type = identifyModel(console.modelName);

    MixerCapabilities caps = MixerCapabilities::forConsole(console.type);
    console.manufacturer = caps.manufacturer;
    console.displayName = caps.displayName;

    const QString consoleName = QString::fromUtf8(fields[2]).trimmed();
    if (!consoleName.isEmpty()) {
        console.displayName = QString("%1 (%2)").arg(caps.displayName, consoleName);
    }

    return console;
}

ConsoleType BehringerWingProbeStrategy::identifyModel(const QString& modelString) const {
    // known tokens: wing-fullsize, wing-compact, wing-rack; all variants speak
    // the same protocol, and a well-formed "WING," reply is always a WING
    Q_UNUSED(modelString);
    return ConsoleType::Wing;
}

} // namespace OpenMix
