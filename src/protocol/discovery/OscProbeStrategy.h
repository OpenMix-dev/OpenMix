#pragma once

#include "../MixerCapabilities.h"
#include "DiscoveredConsole.h"
#include <QHostAddress>
#include <QString>
#include <QVariant>
#include <memory>

namespace OpenMix {

class OscProbeStrategy {
  public:
    virtual ~OscProbeStrategy() = default;

    virtual int probePort() const = 0;

    virtual QString probeCommand() const = 0;

    virtual bool canParseResponse(const QString& path) const = 0;

    virtual DiscoveredConsole parseResponse(const QString& path, const QVariant& value,
                                            const QHostAddress& sender, int senderPort) = 0;

    virtual Manufacturer manufacturer() const = 0;

    virtual QString strategyName() const = 0;
};

using OscProbeStrategyPtr = std::shared_ptr<OscProbeStrategy>;

} // namespace OpenMix
