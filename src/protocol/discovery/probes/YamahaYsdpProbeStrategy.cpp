#include "YamahaYsdpProbeStrategy.h"

namespace OpenMix {

namespace {
// reads one length-prefixed field ([len byte][len bytes]) and advances pos;
// returns empty on truncation
QByteArray readLengthPrefixedField(const QByteArray& data, int& pos) {
    if (pos >= data.size()) {
        return {};
    }
    const int len = static_cast<quint8>(data.at(pos));
    ++pos;
    if (len == 0 || pos + len > data.size()) {
        return {};
    }
    QByteArray field = data.mid(pos, len);
    pos += len;
    return field;
}
} // namespace

QByteArray YamahaYsdpProbeStrategy::rawProbe(const QHostAddress& localAddress) const {
    QByteArray probe;
    probe.append("YSDP", 4);
    probe.append('\0');
    probe.append('#');
    probe.append('\0');
    probe.append('\x04');

    // sender IPv4, big-endian (the console addresses its reply using this)
    const quint32 ip = localAddress.toIPv4Address();
    probe.append(static_cast<char>((ip >> 24) & 0xFF));
    probe.append(static_cast<char>((ip >> 16) & 0xFF));
    probe.append(static_cast<char>((ip >> 8) & 0xFF));
    probe.append(static_cast<char>(ip & 0xFF));

    probe.append(18, '\0');
    probe.append('\x08');
    probe.append("_ypa-scp", 8);
    probe.append(2, '\0');

    return probe;
}

DiscoveredConsole YamahaYsdpProbeStrategy::parseResponse(
    [[maybe_unused]] const QString& path, [[maybe_unused]] const QVariantList& args,
    [[maybe_unused]] const QHostAddress& sender, [[maybe_unused]] int senderPort) {
    // YSDP discovery is raw-datagram only
    return {};
}

DiscoveredConsole YamahaYsdpProbeStrategy::parseRawResponse(const QByteArray& data,
                                                            const QHostAddress& sender,
                                                            [[maybe_unused]] int senderPort) {
    DiscoveredConsole console;

    const int serviceIdx = data.indexOf("_ypa-scp");
    if (serviceIdx < 0) {
        return console;
    }

    // fields start 10 bytes past the service name: host, model, version
    int pos = serviceIdx + 10;

    const QByteArray hostField = readLengthPrefixedField(data, pos);
    if (hostField.size() <= 4) {
        return console;
    }

    const QByteArray modelField = readLengthPrefixedField(data, pos);
    if (modelField.size() <= 1) {
        return console;
    }

    const QByteArray versionField = readLengthPrefixedField(data, pos);

    const QString modelString = QString::fromLatin1(modelField).trimmed();

    console.address = sender;
    console.modelName = modelString;
    console.firmwareVersion = QString::fromLatin1(versionField).trimmed();
    console.manufacturer = Manufacturer::Yamaha;
    console.type = identifyModel(modelString);

    if (console.type != ConsoleType::Unknown) {
        MixerCapabilities caps = MixerCapabilities::forConsole(console.type);
        console.displayName = caps.displayName;
        console.port = caps.defaultPort; // SCP over TCP 49280
    }

    return console;
}

ConsoleType YamahaYsdpProbeStrategy::identifyModel(const QString& modelString) const {
    QString model = modelString.toLower();

    // DM7 series (incl. DM7C / DM7 Compact)
    if (model.contains("dm7")) {
        return ConsoleType::DM7;
    }

    // CL series
    if (model.contains("cl5")) {
        return ConsoleType::CL5;
    }
    if (model.contains("cl3")) {
        return ConsoleType::CL3;
    }
    if (model.contains("cl1")) {
        return ConsoleType::CL1;
    }

    // QL series
    if (model.contains("ql5")) {
        return ConsoleType::QL5;
    }
    if (model.contains("ql1")) {
        return ConsoleType::QL1;
    }

    // TF series
    if (model.contains("tf5")) {
        return ConsoleType::TF5;
    }
    if (model.contains("tf3")) {
        return ConsoleType::TF3;
    }
    if (model.contains("tf1")) {
        return ConsoleType::TF1;
    }

    // generic matches
    if (model.contains("tf")) {
        return ConsoleType::TF5;
    }
    if (model.contains("ql")) {
        return ConsoleType::QL5;
    }
    if (model.contains("cl")) {
        return ConsoleType::CL5;
    }

    return ConsoleType::Unknown;
}

} // namespace OpenMix
