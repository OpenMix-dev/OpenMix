#pragma once

#include "../OscProbeStrategy.h"

namespace OpenMix {

class BehringerX32ProbeStrategy : public OscProbeStrategy {
  public:
    int probePort() const override { return 10023; }

    QString probeCommand() const override { return "/xinfo"; }

    bool canParseResponse(const QString& path) const override { return path == "/xinfo"; }

    DiscoveredConsole parseResponse(const QString& path, const QVariantList& args,
                                    const QHostAddress& sender, int senderPort) override;

    Manufacturer manufacturer() const override { return Manufacturer::Behringer; }

    QString strategyName() const override { return "Behringer X32/M32"; }

  private:
    ConsoleType identifyModel(const QString& modelString) const;
};

} // namespace OpenMix
