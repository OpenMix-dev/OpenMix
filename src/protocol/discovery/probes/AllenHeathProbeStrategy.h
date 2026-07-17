#pragma once

#include "../OscProbeStrategy.h"

namespace OpenMix {

// Allen & Heath discovery is two-stage. Stage 1: broadcast a per-family ASCII
// "Find" string to UDP 51320; A&H devices answer from source port 51320.
// Stage 2: a follow-up TCP handshake reports the exact model. A&H uses two
// distinct identify protocols:
//   * SQ family  - TCP 51326, binary 7F 01 / 7F 02 frame with a model byte.
//   * ACE family - TCP 51321, SysEx "DR Box Identification" request. The reply is
//     a handle, not a name; reading that handle back returns the string that
//     identifies dLive ("TLD..."), Avantis ("Bridge"/"Avantis Solo"), or GLD
//     ("GLD...").
// Both are attempted; the one the device answers wins. Control ports differ
// (SQ/GLD MIDI-over-TCP 51325, Avantis/dLive ACE-TCP 51321).
class AllenHeathProbeStrategy : public OscProbeStrategy {
  public:
    int probePort() const override { return 51320; }

    QString probeCommand() const override { return QString(); }

    QList<QByteArray> rawProbes(const QHostAddress& localAddress) const override;

    bool canParseResponse(const QString& path) const override {
        Q_UNUSED(path);
        return false;
    }

    DiscoveredConsole parseResponse(const QString& path, const QVariantList& args,
                                    const QHostAddress& sender, int senderPort) override;

    // A&H announces presence from source port 51320 with no useful UDP payload
    bool matchesReplyPort(int senderPort) const override { return senderPort == 51320; }

    QList<TcpIdentify> tcpIdentifies() const override;

    QByteArray identifyFollowUp(int identifyPort, const QByteArray& firstReply) const override;
    int identifyFollowUpMinBytes(int identifyPort) const override;

    static QByteArray buildAceHandleRead(const QByteArray& handle);

    DiscoveredConsole parseIdentifyResponse(const QByteArray& response, const QHostAddress& sender,
                                            int identifyPort) const override;

    Manufacturer manufacturer() const override { return Manufacturer::AllenHeath; }

    QString strategyName() const override { return "Allen & Heath"; }

    // identify ports (public so tests can target them directly)
    static constexpr int SQ_IDENTIFY_PORT = 51326;
    static constexpr int ACE_IDENTIFY_PORT = 51321;

  private:
    // SQ (TCP 51326): 7F 02 frame, model byte -> SQ-5/6/7
    DiscoveredConsole parseSqIdentify(const QByteArray& response, const QHostAddress& sender) const;
    ConsoleType consoleForSqFamily(int family) const;

    // ACE (TCP 51321): SysEx reply string -> dLive / Avantis / GLD
    DiscoveredConsole parseAceIdentify(const QByteArray& response,
                                       const QHostAddress& sender) const;
};

} // namespace OpenMix
