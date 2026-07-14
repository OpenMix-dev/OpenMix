#pragma once

#include "../OscProbeStrategy.h"

namespace OpenMix {

// Yamaha discovery (YSDP): raw datagram broadcast to UDP 54330 carrying the
// sender's IPv4 and the "_ypa-scp" service name. Consoles reply with a
// datagram containing "_ypa-scp" followed by length-prefixed fields
// (host, model, version). Control is SCP over TCP 49280.
class YamahaYsdpProbeStrategy : public OscProbeStrategy {
  public:
    int probePort() const override { return 54330; }

    QString probeCommand() const override { return QString(); }

    QByteArray rawProbe(const QHostAddress& localAddress) const override;

    bool canParseResponse(const QString& path) const override {
        Q_UNUSED(path);
        return false;
    }

    DiscoveredConsole parseResponse(const QString& path, const QVariantList& args,
                                    const QHostAddress& sender, int senderPort) override;

    bool canParseRawResponse(const QByteArray& data) const override {
        return data.contains("_ypa-scp");
    }

    DiscoveredConsole parseRawResponse(const QByteArray& data, const QHostAddress& sender,
                                       int senderPort) override;

    Manufacturer manufacturer() const override { return Manufacturer::Yamaha; }

    QString strategyName() const override { return "Yamaha TF/QL/CL/DM7"; }

  private:
    ConsoleType identifyModel(const QString& modelString) const;
};

} // namespace OpenMix
