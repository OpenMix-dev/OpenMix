#include "YamahaOscProbeStrategy.h"

namespace OpenMix {

DiscoveredConsole YamahaOscProbeStrategy::parseResponse(const QString& path, const QVariant& value,
                                                        const QHostAddress& sender,
                                                        int senderPort) {
    Q_UNUSED(path);
    Q_UNUSED(senderPort);

    DiscoveredConsole console;
    console.address = sender;
    console.port = 8000; // Yamaha OSC receive port
    console.manufacturer = Manufacturer::Yamaha;

    QString modelString = value.toString();
    console.modelName = modelString;
    console.type = identifyModel(modelString);

    if (console.type != ConsoleType::Unknown) {
        MixerCapabilities caps = MixerCapabilities::forConsole(console.type);
        console.displayName = caps.displayName;
    } else if (!modelString.isEmpty()) {
        // unknown Yamaha model, display as generic
        console.displayName = QString("Yamaha %1").arg(modelString);
    }

    return console;
}

ConsoleType YamahaOscProbeStrategy::identifyModel(const QString& modelString) const {
    QString model = modelString.toLower();

    // DM7 series
    if (model.contains("dm7")) {
        return ConsoleType::DM7;
    }

    // CL series
    if (model.contains("cl5")) {
        return ConsoleType::CL5;
    }
    if (model.contains("cl3")) {
        return ConsoleType::CL3;
    }
    if (model.contains("cl1")) {
        return ConsoleType::CL1;
    }

    // QL series
    if (model.contains("ql5")) {
        return ConsoleType::QL5;
    }
    if (model.contains("ql1")) {
        return ConsoleType::QL1;
    }

    // TF series
    if (model.contains("tf5")) {
        return ConsoleType::TF5;
    }
    if (model.contains("tf3")) {
        return ConsoleType::TF3;
    }
    if (model.contains("tf1")) {
        return ConsoleType::TF1;
    }

    // generic matches
    if (model.contains("tf")) {
        return ConsoleType::TF5;
    }
    if (model.contains("ql")) {
        return ConsoleType::QL5;
    }
    if (model.contains("cl")) {
        return ConsoleType::CL5;
    }

    return ConsoleType::Unknown;
}

} // namespace OpenMix
