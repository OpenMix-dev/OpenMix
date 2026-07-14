#pragma once

#include "../MixerCapabilities.h"
#include "DiscoveredConsole.h"
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <memory>

namespace OpenMix {

// Describes a follow-up TCP query used to identify a console after a UDP probe
// reply. Some consoles (Allen & Heath) only announce presence over UDP and
// require a short binary TCP handshake to report their exact model.
struct TcpIdentify {
    int port = 0;             // 0 = no TCP identify step
    QByteArray request;       // bytes to send once connected
    int minResponseBytes = 0; // wait until at least this many bytes arrive
    int timeoutMs = 300;      // give up if the console is silent this long

    bool isValid() const { return port > 0; }
};

class OscProbeStrategy {
  public:
    virtual ~OscProbeStrategy() = default;

    virtual int probePort() const = 0;

    virtual QString probeCommand() const = 0;

    // raw (non-OSC) probe payload; non-empty means the service sends these
    // bytes verbatim instead of OSC-encoding probeCommand(). localAddress is
    // the sending interface's address, for protocols that embed it (YSDP).
    virtual QByteArray rawProbe(const QHostAddress& localAddress) const {
        Q_UNUSED(localAddress);
        return {};
    }

    // some protocols broadcast several distinct probe payloads on one port
    // (Allen & Heath sends a "Find" string per console family). Defaults to the
    // single rawProbe() payload when non-empty.
    virtual QList<QByteArray> rawProbes(const QHostAddress& localAddress) const {
        QByteArray single = rawProbe(localAddress);
        if (single.isEmpty()) {
            return {};
        }
        return {single};
    }

    virtual bool canParseResponse(const QString& path) const = 0;

    virtual DiscoveredConsole parseResponse(const QString& path, const QVariantList& args,
                                            const QHostAddress& sender, int senderPort) = 0;

    // raw (non-OSC) response handling for consoles that reply with plain datagrams
    virtual bool canParseRawResponse(const QByteArray& data) const {
        Q_UNUSED(data);
        return false;
    }

    virtual DiscoveredConsole parseRawResponse(const QByteArray& data, const QHostAddress& sender,
                                               int senderPort) {
        Q_UNUSED(data);
        Q_UNUSED(sender);
        Q_UNUSED(senderPort);
        return {};
    }

    // reply matched by sender port rather than content (Allen & Heath answers a
    // "Find" from UDP 51320 with no useful payload; the model comes from a
    // follow-up TCP handshake). Strategies that return true for a sender port
    // should also provide tcpIdentifies() descriptors and parseIdentifyResponse().
    virtual bool matchesReplyPort(int senderPort) const {
        Q_UNUSED(senderPort);
        return false;
    }

    // one or more follow-up TCP handshakes to try after a port-matched reply.
    // Allen & Heath uses two distinct identify protocols on different ports
    // (SQ on 51326, the ACE families on 51321); each is attempted and the one
    // the device answers wins.
    virtual QList<TcpIdentify> tcpIdentifies() const { return {}; }

    // parse the response from the handshake sent to identifyPort
    virtual DiscoveredConsole parseIdentifyResponse(const QByteArray& response,
                                                    const QHostAddress& sender,
                                                    int identifyPort) const {
        Q_UNUSED(response);
        Q_UNUSED(sender);
        Q_UNUSED(identifyPort);
        return {};
    }

    virtual Manufacturer manufacturer() const = 0;

    virtual QString strategyName() const = 0;
};

using OscProbeStrategyPtr = std::shared_ptr<OscProbeStrategy>;

} // namespace OpenMix
