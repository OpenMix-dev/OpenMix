#include "ConsoleDiscoveryService.h"
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <cstring>
#include <memory>

namespace OpenMix {

ConsoleDiscoveryService::ConsoleDiscoveryService(QObject* parent) : QObject(parent) {
    connect(&m_socket, &QUdpSocket::readyRead, this, &ConsoleDiscoveryService::onReadyRead);

    m_scanTimer.setSingleShot(true);
    connect(&m_scanTimer, &QTimer::timeout, this, &ConsoleDiscoveryService::onScanTimeout);
}

ConsoleDiscoveryService::~ConsoleDiscoveryService() { stopScan(); }

void ConsoleDiscoveryService::registerStrategy(OscProbeStrategyPtr strategy) {
    m_strategies.append(strategy);
}

void ConsoleDiscoveryService::startScan(int timeoutMs) {
    if (m_scanning) {
        stopScan();
    }

    m_discovered.clear();
    m_scanning = true;

    // IPv4-only socket: dual-stack sockets have platform quirks with IPv4 broadcast
    if (!m_socket.bind(QHostAddress::AnyIPv4, DISCOVERY_SOURCE_PORT) &&
        !m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        m_scanning = false;
        emit scanError("Failed to bind UDP socket for discovery");
        return;
    }

    emit scanStarted();

    sendProbes();

    m_scanTimer.start(timeoutMs);
}

void ConsoleDiscoveryService::stopScan() {
    m_scanTimer.stop();
    cancelIdentifyProbes();
    m_socket.close();
    m_scanning = false;
}

void ConsoleDiscoveryService::cancelIdentifyProbes() {
    for (QTcpSocket* sock : m_identifySockets) {
        if (sock) {
            sock->abort();
            sock->deleteLater();
        }
    }
    m_identifySockets.clear();
    m_identifyProbed.clear();
}

void ConsoleDiscoveryService::sendProbes() {
    // per-interface subnet broadcasts, using each interface's own address for
    // probes that embed the sender IP
    QHostAddress firstLocal;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

            if (firstLocal.isNull()) {
                firstLocal = entry.ip();
            }

            QHostAddress subnetBroadcast = entry.broadcast();
            if (!subnetBroadcast.isNull()) {
                sendProbesTo(subnetBroadcast, entry.ip());
            }
        }
    }

    // global broadcast as a catch-all
    sendProbesTo(QHostAddress(QHostAddress::Broadcast), firstLocal);
}

void ConsoleDiscoveryService::sendProbesTo(const QHostAddress& target,
                                           const QHostAddress& localAddress) {
    for (const auto& strategy : m_strategies) {
        const int port = strategy->probePort();
        const QList<QByteArray> rawPayloads = strategy->rawProbes(localAddress);
        if (rawPayloads.isEmpty()) {
            m_socket.writeDatagram(buildOscMessage(strategy->probeCommand()), target, port);
            continue;
        }
        for (const QByteArray& payload : rawPayloads) {
            m_socket.writeDatagram(payload, target, port);
        }
    }
}

void ConsoleDiscoveryService::probeHost(const QHostAddress& host) {
    if (!m_scanning || host.isNull()) {
        return;
    }

    // pick the local interface address on the target's subnet for probes that
    // embed the sender IP; fall back to the first usable interface
    QHostAddress localAddress;
    QHostAddress firstLocal;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            if (firstLocal.isNull()) {
                firstLocal = entry.ip();
            }
            if (host.isInSubnet(entry.ip(), entry.prefixLength())) {
                localAddress = entry.ip();
            }
        }
    }
    if (localAddress.isNull()) {
        localAddress = firstLocal;
    }

    sendProbesTo(host, localAddress);
}

void ConsoleDiscoveryService::onReadyRead() {
    while (m_socket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket.receiveDatagram();
        processDatagram(datagram.data(), datagram.senderAddress(), datagram.senderPort());
    }
}

void ConsoleDiscoveryService::onScanTimeout() {
    stopScan();
    emit scanFinished();
}

void ConsoleDiscoveryService::processDatagram(const QByteArray& data, const QHostAddress& sender,
                                              int senderPort) {
    if (data.isEmpty()) {
        return;
    }

    // raw (non-OSC) replies first: WING, YSDP, etc.
    for (const auto& strategy : m_strategies) {
        if (strategy->canParseRawResponse(data)) {
            addConsole(strategy->parseRawResponse(data, sender, senderPort));
            return;
        }
    }

    // sender-port-matched replies with follow-up TCP handshakes (Allen & Heath).
    // A device may speak either identify protocol, so every descriptor is tried.
    bool launched = false;
    for (const auto& strategy : m_strategies) {
        if (!strategy->matchesReplyPort(senderPort)) {
            continue;
        }
        for (const TcpIdentify& id : strategy->tcpIdentifies()) {
            if (id.isValid()) {
                launchTcpIdentify(strategy, sender, id);
                launched = true;
            }
        }
    }
    if (launched) {
        return;
    }

    if (data.at(0) == '/') {
        parseOscMessage(data, sender, senderPort);
    }
}

void ConsoleDiscoveryService::launchTcpIdentify(const OscProbeStrategyPtr& strategy,
                                                const QHostAddress& host, const TcpIdentify& id) {
    const QString key = QString("%1:%2").arg(host.toString()).arg(id.port);
    if (m_identifyProbed.contains(key)) {
        return; // already probing or probed this host during the current scan
    }
    m_identifyProbed.insert(key);

    auto* sock = new QTcpSocket(this);
    m_identifySockets.append(sock);

    auto buffer = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);
    auto followUpSent = std::make_shared<bool>(false);

    const int identifyPort = id.port;
    auto finish = [this, sock, buffer, done, strategy, host, identifyPort]() {
        if (*done) {
            return;
        }
        *done = true;

        DiscoveredConsole console = strategy->parseIdentifyResponse(*buffer, host, identifyPort);
        addConsole(console);

        m_identifySockets.removeAll(sock);
        sock->abort();
        sock->deleteLater();
    };

    connect(sock, &QTcpSocket::connected, sock, [sock, id]() { sock->write(id.request); });
    connect(sock, &QTcpSocket::readyRead, sock,
            [sock, buffer, id, finish, followUpSent, strategy, identifyPort]() {
                buffer->append(sock->readAll());

                if (!*followUpSent) {
                    if (buffer->size() < id.minResponseBytes) {
                        return;
                    }
                    const QByteArray followUp = strategy->identifyFollowUp(identifyPort, *buffer);
                    if (!followUp.isEmpty()) {
                        *followUpSent = true;
                        buffer->clear();
                        sock->write(followUp);
                        return;
                    }
                    finish();
                    return;
                }

                if (buffer->size() >= strategy->identifyFollowUpMinBytes(identifyPort)) {
                    finish();
                }
            });
    connect(sock, &QTcpSocket::errorOccurred, sock,
            [finish](QAbstractSocket::SocketError) { finish(); });

    // bound the wait: a silent or non-A&H host must not stall the scan
    QTimer::singleShot(id.timeoutMs, sock, finish);

    sock->connectToHost(host, id.port);
}

void ConsoleDiscoveryService::addConsole(const DiscoveredConsole& in) {
    if (!in.isValid()) {
        return;
    }

    // A dual-stack socket reports an IPv4 sender as ::ffff:a.b.c.d, which then
    // reaches the driver as that literal string. Store the plain IPv4 form so
    // everything downstream sees an address the console will answer on.
    DiscoveredConsole console = in;
    bool isV4 = false;
    const quint32 v4 = console.address.toIPv4Address(&isV4);
    if (isV4) {
        console.address = QHostAddress(v4);
    }

    for (const auto& existing : m_discovered) {
        if (existing == console) {
            return;
        }
    }

    m_discovered.append(console);
    emit consoleDiscovered(console);
}

void ConsoleDiscoveryService::parseOscMessage(const QByteArray& data, const QHostAddress& sender,
                                              int senderPort) {
    if (data.size() < 4) {
        return;
    }

    // extract OSC path
    int pathEnd = data.indexOf('\0');
    if (pathEnd < 0) {
        return;
    }
    QString path = QString::fromUtf8(data.left(pathEnd));

    // skip to type tag (4-byte aligned)
    int typeStart = ((pathEnd + 4) / 4) * 4;
    QVariantList args;

    if (typeStart < data.size() && data.at(typeStart) == ',') {
        int typeEnd = data.indexOf('\0', typeStart);
        if (typeEnd > typeStart) {
            QString types = QString::fromUtf8(data.mid(typeStart + 1, typeEnd - typeStart - 1));
            int argOffset = ((typeEnd + 4) / 4) * 4;

            for (const QChar& type : types) {
                if (argOffset > data.size()) {
                    break;
                }
                QVariant value = parseOscArgument(data, argOffset, type.toLatin1());
                if (!value.isValid()) {
                    break;
                }
                args.append(value);
            }
        }
    }

    // try each strategy to parse the response
    for (const auto& strategy : m_strategies) {
        if (strategy->canParseResponse(path)) {
            addConsole(strategy->parseResponse(path, args, sender, senderPort));
            break;
        }
    }
}

QVariant ConsoleDiscoveryService::parseOscArgument(const QByteArray& data, int& offset, char type) {
    switch (type) {
    case 'f': {
        if (offset + 4 <= data.size()) {
            quint32 raw = (static_cast<quint8>(data[offset]) << 24) |
                          (static_cast<quint8>(data[offset + 1]) << 16) |
                          (static_cast<quint8>(data[offset + 2]) << 8) |
                          static_cast<quint8>(data[offset + 3]);
            float f;
            std::memcpy(&f, &raw, sizeof(float));
            offset += 4;
            return f;
        }
        break;
    }
    case 'i': {
        if (offset + 4 <= data.size()) {
            qint32 i = (static_cast<quint8>(data[offset]) << 24) |
                       (static_cast<quint8>(data[offset + 1]) << 16) |
                       (static_cast<quint8>(data[offset + 2]) << 8) |
                       static_cast<quint8>(data[offset + 3]);
            offset += 4;
            return i;
        }
        break;
    }
    case 's': {
        int strEnd = data.indexOf('\0', offset);
        if (strEnd >= offset) {
            QString str = QString::fromUtf8(data.mid(offset, strEnd - offset));
            offset = ((strEnd + 4) / 4) * 4;
            return str;
        }
        break;
    }
    case 'b': {
        if (offset + 4 <= data.size()) {
            qint32 size = (static_cast<quint8>(data[offset]) << 24) |
                          (static_cast<quint8>(data[offset + 1]) << 16) |
                          (static_cast<quint8>(data[offset + 2]) << 8) |
                          static_cast<quint8>(data[offset + 3]);
            offset += 4;
            if (size >= 0 && offset + size <= data.size()) {
                QByteArray blob = data.mid(offset, size);
                offset += ((size + 3) / 4) * 4;
                return blob;
            }
        }
        break;
    }
    }

    return QVariant();
}

QByteArray ConsoleDiscoveryService::buildOscMessage(const QString& path) {
    QByteArray msg;

    // path (null-terminated, padded to 4-byte boundary)
    QByteArray pathBytes = path.toUtf8();
    pathBytes.append('\0');
    while (pathBytes.size() % 4 != 0) {
        pathBytes.append('\0');
    }
    msg.append(pathBytes);

    // type tag (no arguments)
    QByteArray typeTag = ",";
    typeTag.append('\0');
    while (typeTag.size() % 4 != 0) {
        typeTag.append('\0');
    }
    msg.append(typeTag);

    return msg;
}

} // namespace OpenMix
