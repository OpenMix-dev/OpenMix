#include "ProtocolFactory.h"
#include "LoopbackProtocol.h"
#include "MixerProtocol.h"
#include "discovery/DiscoveredConsole.h"

#include "allenheath/AvantisProtocol.h"
#include "allenheath/DLiveProtocol.h"
#include "allenheath/GLDProtocol.h"
#include "allenheath/SQProtocol.h"
#include "behringer/WingProtocol.h"
#include "behringer/X32Protocol.h"
#include "yamaha/YamahaCLProtocol.h"
#include "yamaha/YamahaDM7Protocol.h"
#include "yamaha/YamahaQLProtocol.h"
#include "yamaha/YamahaTFProtocol.h"

namespace OpenMix {

MixerProtocol* ProtocolFactory::create(const QString& type, QObject* parent) {
    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);
    return create(caps, parent);
}

MixerProtocol* ProtocolFactory::create(ConsoleType type, QObject* parent) {
    MixerCapabilities caps = MixerCapabilities::forConsole(type);
    return create(caps, parent);
}

MixerProtocol* ProtocolFactory::create(const MixerCapabilities& caps, QObject* parent) {
    switch (caps.type) {
    case ConsoleType::X32:
    case ConsoleType::M32: // Midas M32 is X32-compatible
        return new X32Protocol(caps, parent);

    case ConsoleType::Wing:
        return new WingProtocol(caps, parent);

    case ConsoleType::SQ5:
    case ConsoleType::SQ6:
    case ConsoleType::SQ7:
        return new SQProtocol(caps, parent);

    case ConsoleType::GLD80:
    case ConsoleType::GLD112:
        return new GLDProtocol(caps, parent);

    case ConsoleType::Avantis:
        return new AvantisProtocol(caps, parent);

    case ConsoleType::DLive:
        return new DLiveProtocol(caps, parent);

    case ConsoleType::TF1:
    case ConsoleType::TF3:
    case ConsoleType::TF5:
        return new YamahaTFProtocol(caps, parent);

    case ConsoleType::QL1:
    case ConsoleType::QL5:
        return new YamahaQLProtocol(caps, parent);

    case ConsoleType::CL1:
    case ConsoleType::CL3:
    case ConsoleType::CL5:
        return new YamahaCLProtocol(caps, parent);

    case ConsoleType::DM7:
        return new YamahaDM7Protocol(caps, parent);

    case ConsoleType::Loopback:
        return new LoopbackProtocol(caps, parent);

    default:
        return nullptr;
    }
}

bool ProtocolFactory::isImplemented(const QString& type) {
    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);
    return isImplemented(caps.type);
}

bool ProtocolFactory::isImplemented(ConsoleType type) {
    switch (type) {
    case ConsoleType::X32:
    case ConsoleType::M32:
    case ConsoleType::Wing:
    case ConsoleType::SQ5:
    case ConsoleType::SQ6:
    case ConsoleType::SQ7:
    case ConsoleType::GLD80:
    case ConsoleType::GLD112:
    case ConsoleType::Avantis:
    case ConsoleType::DLive:
    case ConsoleType::TF1:
    case ConsoleType::TF3:
    case ConsoleType::TF5:
    case ConsoleType::QL1:
    case ConsoleType::QL5:
    case ConsoleType::CL1:
    case ConsoleType::CL3:
    case ConsoleType::CL5:
    case ConsoleType::DM7:
    case ConsoleType::Loopback:
        return true;
    default:
        return false;
    }
}

MixerProtocol* ProtocolFactory::create(const DiscoveredConsole& console, QObject* parent) {
    MixerCapabilities caps = console.toCapabilities();
    return create(caps, parent);
}

MixerCapabilities ProtocolFactory::capabilities(const QString& type) {
    return MixerCapabilities::forProtocolId(type);
}

} // namespace OpenMix
