#pragma once

#include "../OscProbeStrategy.h"

namespace OpenMix {

class YamahaOscProbeStrategy : public OscProbeStrategy {
  public:
    int probePort() const override { return 8000; }

    QString probeCommand() const override { return "/sys/model"; }

    bool canParseResponse(const QString& path) const override { return path == "/sys/model"; }

    DiscoveredConsole parseResponse(const QString& path, const QVariant& value,
                                    const QHostAddress& sender, int senderPort) override;

    Manufacturer manufacturer() const override { return Manufacturer::Yamaha; }

    QString strategyName() const override { return "Yamaha TF/QL/CL/DM7"; }

  private:
    ConsoleType identifyModel(const QString& modelString) const;
};

} // namespace OpenMix
