#pragma once

#include "MixerCapabilities.h"
#include "discovery/DiscoveredConsole.h"
#include <QObject>

namespace OpenMix {

class MixerProtocol;

class ProtocolFactory {
  public:
    [[nodiscard]] static MixerProtocol* create(const QString& type, QObject* parent = nullptr);
    [[nodiscard]] static MixerProtocol* create(ConsoleType type, QObject* parent = nullptr);
    [[nodiscard]] static MixerProtocol* create(const MixerCapabilities& caps, QObject* parent = nullptr);
    [[nodiscard]] static MixerProtocol* create(const DiscoveredConsole& console, QObject* parent = nullptr);

    [[nodiscard]] static bool isImplemented(const QString& type);
    [[nodiscard]] static bool isImplemented(ConsoleType type);

    [[nodiscard]] static MixerCapabilities capabilities(const QString& type);

  private:
    ProtocolFactory() = default;
};

} // namespace OpenMix
