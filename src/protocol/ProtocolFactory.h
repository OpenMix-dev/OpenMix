#pragma once

#include "MixerCapabilities.h"
#include <QObject>

namespace OpenMix {

class MixerProtocol;

class ProtocolFactory {
  public:
    static MixerProtocol* create(const QString& type, QObject* parent = nullptr);
    static MixerProtocol* create(ConsoleType type, QObject* parent = nullptr);
    static MixerProtocol* create(const MixerCapabilities& caps, QObject* parent = nullptr);

    static bool isImplemented(const QString& type);
    static bool isImplemented(ConsoleType type);

    static MixerCapabilities capabilities(const QString& type);

  private:
    ProtocolFactory() = default;
};

} // namespace OpenMix
