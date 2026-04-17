#include "MixerCapabilities.h"

namespace OpenMix {

MixerCapabilities MixerCapabilities::forConsole(ConsoleType type) {
    MixerCapabilities caps;
    caps.type = type;

    switch (type) {
    case ConsoleType::X32:
        caps.manufacturer = Manufacturer::Behringer;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Behringer X32";
        caps.protocolId = "x32";
        caps.defaultPort = 10023;
        caps.dcaCount = 8;
        caps.inputChannels = 32;
        caps.mixBuses = 16;
        caps.matrixOutputs = 6;
        caps.scenes = 100;
        caps.maxDCANameLength = 12;
        caps.eqBandsPerChannel = 6;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LCut", "LShv", "PEQ", "VEQ", "HShv", "HCut"};
        // 16 mix buses can be used as effect sends
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::M32:
        caps.manufacturer = Manufacturer::Midas;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Midas M32";
        caps.protocolId = "m32";
        caps.defaultPort = 10023;
        caps.dcaCount = 8;
        caps.inputChannels = 32;
        caps.mixBuses = 16;
        caps.matrixOutputs = 6;
        caps.scenes = 100;
        caps.maxDCANameLength = 12;
        caps.eqBandsPerChannel = 6;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LCut", "LShv", "PEQ", "VEQ", "HShv", "HCut"};
        // 16 mix buses
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::Wing:
        caps.manufacturer = Manufacturer::Behringer;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Behringer WING";
        caps.protocolId = "wing";
        caps.defaultPort = 2223;
        caps.dcaCount = 24;
        caps.inputChannels = 48;
        caps.mixBuses = 16;
        caps.matrixOutputs = 8;
        caps.scenes = 100;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 6;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LCut", "LShv", "PEQ", "VEQ", "HShv", "HCut"};
        // 16 effect send buses
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    // Allen & Heath
    case ConsoleType::SQ5:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::MidiTcp;
        caps.displayName = "Allen & Heath SQ-5";
        caps.protocolId = "sq5";
        caps.defaultPort = 51325;
        caps.dcaCount = 8;
        caps.inputChannels = 48;
        caps.mixBuses = 12;
        caps.matrixOutputs = 3;
        caps.scenes = 300;
        caps.maxDCANameLength = 6;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 4 dedicated FX sends
        caps.effectSendBuses = 4;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::SQ6:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::MidiTcp;
        caps.displayName = "Allen & Heath SQ-6";
        caps.protocolId = "sq6";
        caps.defaultPort = 51325;
        caps.dcaCount = 8;
        caps.inputChannels = 48;
        caps.mixBuses = 12;
        caps.matrixOutputs = 3;
        caps.scenes = 300;
        caps.maxDCANameLength = 6;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 4 dedicated FX sends
        caps.effectSendBuses = 4;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::SQ7:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::MidiTcp;
        caps.displayName = "Allen & Heath SQ-7";
        caps.protocolId = "sq7";
        caps.defaultPort = 51325;
        caps.dcaCount = 8;
        caps.inputChannels = 48;
        caps.mixBuses = 12;
        caps.matrixOutputs = 3;
        caps.scenes = 300;
        caps.maxDCANameLength = 6;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 4 dedicated FX sends
        caps.effectSendBuses = 4;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::GLD80:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::MidiTcp;
        caps.displayName = "Allen & Heath GLD-80";
        caps.protocolId = "gld80";
        caps.defaultPort = 51325;
        caps.dcaCount = 8;
        caps.inputChannels = 48;
        caps.mixBuses = 20;
        caps.matrixOutputs = 4;
        caps.scenes = 250;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 8 dedicated FX sends
        caps.effectSendBuses = 8;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::GLD112:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::MidiTcp;
        caps.displayName = "Allen & Heath GLD-112";
        caps.protocolId = "gld112";
        caps.defaultPort = 51325;
        caps.dcaCount = 8;
        caps.inputChannels = 48;
        caps.mixBuses = 30;
        caps.matrixOutputs = 4;
        caps.scenes = 250;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 8 dedicated FX sends
        caps.effectSendBuses = 8;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::Avantis:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::BinaryTcp;
        caps.displayName = "Allen & Heath Avantis";
        caps.protocolId = "avantis";
        caps.defaultPort = 51321;
        caps.dcaCount = 16;
        caps.inputChannels = 64;
        caps.mixBuses = 36;
        caps.matrixOutputs = 6;
        caps.scenes = 500;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 8 dedicated FX sends
        caps.effectSendBuses = 8;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::DLive:
        caps.manufacturer = Manufacturer::AllenHeath;
        caps.protocol = ProtocolType::BinaryTcp;
        caps.displayName = "Allen & Heath dLive";
        caps.protocolId = "dlive";
        caps.defaultPort = 51321;
        caps.dcaCount = 16;
        caps.inputChannels = 128;
        caps.mixBuses = 64;
        caps.matrixOutputs = 16;
        caps.scenes = 500;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LShv", "PEQ", "PEQ", "HShv"};
        // 16 dedicated FX sends
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    // Yamaha
    case ConsoleType::TF1:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha TF1";
        caps.protocolId = "tf1";
        caps.defaultPort = 8000;
        caps.dcaCount = 8;
        caps.inputChannels = 16;
        caps.mixBuses = 20;
        caps.matrixOutputs = 0;
        caps.scenes = 100;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 8 effect sends (4 dedicated + 4 matrix)
        caps.effectSendBuses = 8;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::TF3:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha TF3";
        caps.protocolId = "tf3";
        caps.defaultPort = 8000;
        caps.dcaCount = 8;
        caps.inputChannels = 24;
        caps.mixBuses = 20;
        caps.matrixOutputs = 0;
        caps.scenes = 100;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 8 effect sends
        caps.effectSendBuses = 8;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::TF5:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha TF5";
        caps.protocolId = "tf5";
        caps.defaultPort = 8000;
        caps.dcaCount = 8;
        caps.inputChannels = 32;
        caps.mixBuses = 20;
        caps.matrixOutputs = 0;
        caps.scenes = 100;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 8 effect sends
        caps.effectSendBuses = 8;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::QL1:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha QL1";
        caps.protocolId = "ql1";
        caps.defaultPort = 8000;
        caps.dcaCount = 8;
        caps.inputChannels = 32;
        caps.mixBuses = 16;
        caps.matrixOutputs = 8;
        caps.scenes = 300;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 16 mix buses as effect sends
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::QL5:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha QL5";
        caps.protocolId = "ql5";
        caps.defaultPort = 8000;
        caps.dcaCount = 8;
        caps.inputChannels = 64;
        caps.mixBuses = 16;
        caps.matrixOutputs = 8;
        caps.scenes = 300;
        caps.maxDCANameLength = 8;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 16 mix buses as effect sends
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::CL1:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha CL1";
        caps.protocolId = "cl1";
        caps.defaultPort = 8000;
        caps.dcaCount = 16;
        caps.inputChannels = 48;
        caps.mixBuses = 24;
        caps.matrixOutputs = 8;
        caps.scenes = 300;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 24 mix buses as effect sends
        caps.effectSendBuses = 24;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::CL3:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha CL3";
        caps.protocolId = "cl3";
        caps.defaultPort = 8000;
        caps.dcaCount = 16;
        caps.inputChannels = 64;
        caps.mixBuses = 24;
        caps.matrixOutputs = 8;
        caps.scenes = 300;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 24 mix buses as effect sends
        caps.effectSendBuses = 24;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::CL5:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha CL5";
        caps.protocolId = "cl5";
        caps.defaultPort = 8000;
        caps.dcaCount = 16;
        caps.inputChannels = 72;
        caps.mixBuses = 24;
        caps.matrixOutputs = 8;
        caps.scenes = 300;
        caps.eqBandsPerChannel = 4;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "HShv"};
        // 24 mix buses as effect sends
        caps.effectSendBuses = 24;
        caps.supportsEffectSends = true;
        break;

    case ConsoleType::DM7:
        caps.manufacturer = Manufacturer::Yamaha;
        caps.protocol = ProtocolType::OscUdp;
        caps.displayName = "Yamaha DM7";
        caps.protocolId = "dm7";
        caps.defaultPort = 8000;
        caps.dcaCount = 24;
        caps.inputChannels = 120;
        caps.mixBuses = 48;
        caps.matrixOutputs = 24;
        caps.scenes = 300;
        caps.eqBandsPerChannel = 6;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"HPF", "LShv", "PEQ", "PEQ", "HShv", "LPF"};
        // 48 mix buses as effect sends
        caps.effectSendBuses = 48;
        caps.supportsEffectSends = true;
        break;

    // Loopback
    case ConsoleType::Loopback:
        caps.manufacturer = Manufacturer::Unknown;
        caps.protocol = ProtocolType::Internal;
        caps.displayName = "Loopback (Test)";
        caps.protocolId = "loopback";
        caps.defaultPort = 0;
        caps.dcaCount = 8;
        caps.inputChannels = 32;
        caps.mixBuses = 16;
        caps.matrixOutputs = 6;
        caps.scenes = 100;
        caps.maxDCANameLength = 12;
        caps.eqBandsPerChannel = 6;
        caps.supportsChannelEQ = true;
        caps.eqBandTypes = {"LCut", "LShv", "PEQ", "VEQ", "HShv", "HCut"};
        // 16 effect send buses (for testing)
        caps.effectSendBuses = 16;
        caps.supportsEffectSends = true;
        break;

    default:
        caps.manufacturer = Manufacturer::Unknown;
        caps.displayName = "Unknown Console";
        caps.protocolId = "unknown";
        break;
    }

    return caps;
}

MixerCapabilities MixerCapabilities::forProtocolId(const QString& protocolId) {
    QString id = protocolId.toLower();

    if (id == "x32")
        return forConsole(ConsoleType::X32);
    if (id == "m32" || id == "midas")
        return forConsole(ConsoleType::M32);
    if (id == "wing")
        return forConsole(ConsoleType::Wing);

    // Allen & Heath SQ
    if (id == "sq5" || id == "sq-5")
        return forConsole(ConsoleType::SQ5);
    if (id == "sq6" || id == "sq-6")
        return forConsole(ConsoleType::SQ6);
    if (id == "sq7" || id == "sq-7" || id == "sq")
        return forConsole(ConsoleType::SQ7);

    // Allen & Heath GLD
    if (id == "gld80" || id == "gld-80")
        return forConsole(ConsoleType::GLD80);
    if (id == "gld112" || id == "gld-112" || id == "gld")
        return forConsole(ConsoleType::GLD112);

    // Allen & Heath Avantis/dLive
    if (id == "avantis")
        return forConsole(ConsoleType::Avantis);
    if (id == "dlive" || id == "d-live")
        return forConsole(ConsoleType::DLive);

    // Yamaha TF
    if (id == "tf1")
        return forConsole(ConsoleType::TF1);
    if (id == "tf3")
        return forConsole(ConsoleType::TF3);
    if (id == "tf5" || id == "tf")
        return forConsole(ConsoleType::TF5);

    // Yamaha QL
    if (id == "ql1")
        return forConsole(ConsoleType::QL1);
    if (id == "ql5" || id == "ql")
        return forConsole(ConsoleType::QL5);

    // Yamaha CL
    if (id == "cl1")
        return forConsole(ConsoleType::CL1);
    if (id == "cl3")
        return forConsole(ConsoleType::CL3);
    if (id == "cl5" || id == "cl")
        return forConsole(ConsoleType::CL5);

    // Yamaha DM7
    if (id == "dm7")
        return forConsole(ConsoleType::DM7);

    // Loopback
    if (id == "loopback" || id == "test")
        return forConsole(ConsoleType::Loopback);

    return forConsole(ConsoleType::Unknown);
}

QVector<MixerCapabilities> MixerCapabilities::allSupported() {
    QVector<MixerCapabilities> all;

    all.append(forConsole(ConsoleType::X32));
    all.append(forConsole(ConsoleType::M32));
    all.append(forConsole(ConsoleType::Wing));

    // Allen & Heath
    all.append(forConsole(ConsoleType::SQ5));
    all.append(forConsole(ConsoleType::SQ6));
    all.append(forConsole(ConsoleType::SQ7));
    all.append(forConsole(ConsoleType::GLD80));
    all.append(forConsole(ConsoleType::GLD112));

    all.append(forConsole(ConsoleType::Avantis));
    all.append(forConsole(ConsoleType::DLive));

    // Yamaha
    all.append(forConsole(ConsoleType::TF1));
    all.append(forConsole(ConsoleType::TF3));
    all.append(forConsole(ConsoleType::TF5));
    all.append(forConsole(ConsoleType::QL1));
    all.append(forConsole(ConsoleType::QL5));
    all.append(forConsole(ConsoleType::CL1));
    all.append(forConsole(ConsoleType::CL3));
    all.append(forConsole(ConsoleType::CL5));
    all.append(forConsole(ConsoleType::DM7));

    return all;
}

QVector<MixerCapabilities> MixerCapabilities::forManufacturer(Manufacturer manufacturer) {
    QVector<MixerCapabilities> result;
    for (const auto& caps : allSupported()) {
        if (caps.manufacturer == manufacturer) {
            result.append(caps);
        }
    }
    return result;
}

bool MixerCapabilities::isSupported() const {
    switch (type) {
    case ConsoleType::Loopback:
        return true;

    // Behringer / Midas
    case ConsoleType::X32:
    case ConsoleType::M32:
        return true;

    // Allen & Heath SQ/GLD
    case ConsoleType::SQ5:
    case ConsoleType::SQ6:
    case ConsoleType::SQ7:
    case ConsoleType::GLD80:
    case ConsoleType::GLD112:
        return true;

    // Yamaha TF series
    case ConsoleType::TF1:
    case ConsoleType::TF3:
    case ConsoleType::TF5:
        return true;

    // Allen & Heath Avantis/dLive
    case ConsoleType::Avantis:
    case ConsoleType::DLive:
        return true;

    // Yamaha QL/CL/DM7
    case ConsoleType::QL1:
    case ConsoleType::QL5:
    case ConsoleType::CL1:
    case ConsoleType::CL3:
    case ConsoleType::CL5:
    case ConsoleType::DM7:
        return true;

    // Behringer Wing
    case ConsoleType::Wing:
        return true;

    default:
        return false;
    }
}

QString MixerCapabilities::manufacturerName() const {
    switch (manufacturer) {
    case Manufacturer::Behringer:
        return "Behringer";
    case Manufacturer::Midas:
        return "Midas";
    case Manufacturer::AllenHeath:
        return "Allen & Heath";
    case Manufacturer::Yamaha:
        return "Yamaha";
    default:
        return "Unknown";
    }
}

QString consoleTypeToString(ConsoleType type) {
    return MixerCapabilities::forConsole(type).protocolId;
}

ConsoleType stringToConsoleType(const QString& str) {
    return MixerCapabilities::forProtocolId(str).type;
}

QString protocolTypeToString(ProtocolType type) {
    switch (type) {
    case ProtocolType::OscUdp:
        return "OSC/UDP";
    case ProtocolType::MidiTcp:
        return "MIDI/TCP";
    case ProtocolType::BinaryTcp:
        return "Binary/TCP";
    case ProtocolType::TextTcp:
        return "Text/TCP";
    case ProtocolType::Internal:
        return "Internal";
    default:
        return "Unknown";
    }
}

} // namespace OpenMix
