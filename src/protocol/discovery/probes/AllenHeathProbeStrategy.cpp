#include "AllenHeathProbeStrategy.h"

namespace OpenMix {

namespace {

// builds an ACE SysEx property request: F0 00 01 00 00 00 01 00 04 00 <len>
// <ascii> 00 F7, where <len> counts the ASCII bytes plus the trailing NUL
QByteArray aceRequest(const QByteArray& property) {
    QByteArray req = QByteArray::fromHex("f0000100000001000400");
    req.append(static_cast<char>(property.size() + 1)); // + NUL
    req.append(property);
    req.append('\0');
    req.append('\xf7');
    return req;
}

// extracts the ASCII identity string from an ACE reply frame:
// require F0 00 prefix, strip the 11-byte header, drop the trailing F7
QString aceIdentityString(const QByteArray& response) {
    if (response.size() < 13 || static_cast<quint8>(response.at(0)) != 0xf0 ||
        static_cast<quint8>(response.at(1)) != 0x00) {
        return {};
    }
    QByteArray body = response.mid(11);
    if (!body.isEmpty() && static_cast<quint8>(body.back()) == 0xf7) {
        body.chop(1);
    }
    return QString::fromLatin1(body);
}

// version = substring after " V", up to " - "; revision = after " - Rev. "
QString aceFirmware(const QString& identity) {
    QString firmware;
    const int vIdx = identity.indexOf(" V");
    if (vIdx >= 0) {
        int end = identity.indexOf(" - ", vIdx);
        if (end < 0) {
            end = identity.size();
        }
        firmware = identity.mid(vIdx + 2, end - (vIdx + 2)).trimmed();
    }
    const int rIdx = identity.indexOf(" - Rev. ");
    if (rIdx >= 0) {
        const QString rev = identity.mid(rIdx + 8).trimmed();
        firmware = firmware.isEmpty() ? QString("Rev. %1").arg(rev)
                                      : QString("%1 (Rev. %2)").arg(firmware, rev);
    }
    return firmware;
}

// maps a dLive "TLDDM<n>Stagebox" identity token to a short model code; compact
// (DMC) variants must be checked before the plain DM ones
QString dliveModelCode(const QString& identity) {
    struct Token {
        const char* token;
        const char* code;
    };
    static const Token tokens[] = {
        {"TLDDMC32Stagebox", "CDM32"}, {"TLDDMC48Stagebox", "CDM48"}, {"TLDDMC64Stagebox", "CDM64"},
        {"TLDDM0Stagebox", "DM0"},     {"TLDDM32Stagebox", "DM32"},   {"TLDDM48Stagebox", "DM48"},
        {"TLDDM64Stagebox", "DM64"},
    };
    for (const Token& t : tokens) {
        if (identity.startsWith(QLatin1String(t.token))) {
            return QString::fromLatin1(t.code);
        }
    }
    return {};
}

} // namespace

QList<QByteArray> AllenHeathProbeStrategy::rawProbes(const QHostAddress& localAddress) const {
    Q_UNUSED(localAddress);
    // one broadcast per console family, as sent by the reference implementation
    return {QByteArrayLiteral("SQ Find"), QByteArrayLiteral("Qu-567 Find"),
            QByteArrayLiteral("GLD Find"), QByteArrayLiteral("Bridge Find"),
            QByteArrayLiteral("TLD Find")};
}

DiscoveredConsole AllenHeathProbeStrategy::parseResponse(
    [[maybe_unused]] const QString& path, [[maybe_unused]] const QVariantList& args,
    [[maybe_unused]] const QHostAddress& sender, [[maybe_unused]] int senderPort) {
    // A&H discovery does not use OSC
    return {};
}

QList<TcpIdentify> AllenHeathProbeStrategy::tcpIdentifies() const {
    TcpIdentify sq;
    sq.port = SQ_IDENTIFY_PORT; // 51326
    sq.request = QByteArray::fromHex("7f0100000000");
    sq.minResponseBytes = 12; // 6-byte header + family + 3 version octets + 2 revision
    sq.timeoutMs = 300;

    TcpIdentify ace;
    ace.port = ACE_IDENTIFY_PORT; // 51321
    ace.request = aceRequest(QByteArrayLiteral("DR Box Identification"));
    ace.minResponseBytes = 13; // F0 + 10 header + at least one payload byte + F7
    ace.timeoutMs = 300;

    return {sq, ace};
}

DiscoveredConsole AllenHeathProbeStrategy::parseIdentifyResponse(const QByteArray& response,
                                                                 const QHostAddress& sender,
                                                                 int identifyPort) const {
    if (identifyPort == SQ_IDENTIFY_PORT) {
        return parseSqIdentify(response, sender);
    }
    if (identifyPort == ACE_IDENTIFY_PORT) {
        return parseAceIdentify(response, sender);
    }
    return {};
}

DiscoveredConsole AllenHeathProbeStrategy::parseSqIdentify(const QByteArray& response,
                                                           const QHostAddress& sender) const {
    DiscoveredConsole console;

    // response frame: [7F 02 xx xx xx xx][family][vMaj][vMin][vPatch][rev:2 LE]
    if (response.size() < 12 || static_cast<quint8>(response.at(0)) != 0x7f ||
        static_cast<quint8>(response.at(1)) != 0x02) {
        return console;
    }

    const int family = static_cast<quint8>(response.at(6));
    const ConsoleType type = consoleForSqFamily(family);
    if (type == ConsoleType::Unknown) {
        return console; // e.g. Qu, which OpenMix does not model
    }

    const int major = static_cast<quint8>(response.at(7));
    const int minor = static_cast<quint8>(response.at(8));
    const int patch = static_cast<quint8>(response.at(9));

    MixerCapabilities caps = MixerCapabilities::forConsole(type);
    console.address = sender;
    console.type = type;
    console.port = caps.defaultPort; // MIDI-over-TCP control port (51325)
    console.manufacturer = caps.manufacturer;
    console.displayName = caps.displayName;
    console.modelName = caps.displayName;
    console.firmwareVersion = QString("%1.%2.%3").arg(major).arg(minor).arg(patch);

    return console;
}

ConsoleType AllenHeathProbeStrategy::consoleForSqFamily(int family) const {
    switch (family) {
    case 1:
        return ConsoleType::SQ5;
    case 2:
        return ConsoleType::SQ6;
    case 3:
        return ConsoleType::SQ7;
    // families 8/9/10 are the reference's Qu-5/6/7 = Qu-16/24/32 by fader count
    case 8:
        return ConsoleType::Qu16;
    case 9:
        return ConsoleType::Qu24;
    case 10:
        return ConsoleType::Qu32;
    default:
        return ConsoleType::Unknown;
    }
}

DiscoveredConsole AllenHeathProbeStrategy::parseAceIdentify(const QByteArray& response,
                                                            const QHostAddress& sender) const {
    DiscoveredConsole console;

    const QString identity = aceIdentityString(response);
    if (identity.isEmpty()) {
        return console;
    }

    ConsoleType type = ConsoleType::Unknown;
    QString modelName;

    if (identity.startsWith(QLatin1String("TLD"))) {
        // dLive mixrack; the size variant is display-only (OpenMix has one dLive type)
        type = ConsoleType::DLive;
        const QString code = dliveModelCode(identity);
        modelName = code.isEmpty() ? QStringLiteral("dLive") : QString("dLive %1").arg(code);
    } else if (identity.startsWith(QLatin1String("Bridge"))) {
        type = ConsoleType::Avantis;
        modelName = QStringLiteral("Avantis");
    } else if (identity.startsWith(QLatin1String("Avantis Solo"))) {
        type = ConsoleType::Avantis;
        modelName = QStringLiteral("Avantis Solo");
    }
    // GLD is intentionally not identified here. Unlike dLive/Avantis it does not
    // answer this ACE handshake: GLD identification is a stateful MIDI-over-TCP
    // (51325) "box-walk" that resolves the exact GLD-80/112 model only via a
    // follow-up query keyed on a runtime box id, which a single-shot probe
    // cannot perform. GLD stays a connect-time selection.

    if (type == ConsoleType::Unknown) {
        return console;
    }

    MixerCapabilities caps = MixerCapabilities::forConsole(type);
    console.address = sender;
    console.type = type;
    console.port = caps.defaultPort; // ACE control port (51321)
    console.manufacturer = caps.manufacturer;
    console.modelName = modelName;
    console.displayName = modelName;
    console.firmwareVersion = aceFirmware(identity);

    return console;
}

} // namespace OpenMix
