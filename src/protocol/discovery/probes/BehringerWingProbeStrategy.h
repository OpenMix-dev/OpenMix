#pragma once

#include "../OscProbeStrategy.h"

namespace OpenMix {

class BehringerWingProbeStrategy : public OscProbeStrategy {
  public:
    int probePort() const override { return 2223; }

    QString probeCommand() const override { return "/$info"; }

    bool canParseResponse(const QString& path) const override {
        return path == "/$info" || path.startsWith("/$");
    }

    DiscoveredConsole parseResponse(const QString& path, const QVariant& value,
                                    const QHostAddress& sender, int senderPort) override;

    Manufacturer manufacturer() const override { return Manufacturer::Behringer; }

    QString strategyName() const override { return "Behringer WING"; }

  private:
    ConsoleType identifyModel(const QString& modelString) const;
};

} // namespace OpenMix
