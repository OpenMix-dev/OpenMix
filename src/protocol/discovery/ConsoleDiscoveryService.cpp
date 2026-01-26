#include "ConsoleDiscoveryService.h"
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <cstring>

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

    if (!m_socket.bind(QHostAddress::Any, 0)) {
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
    m_socket.close();
    m_scanning = false;
}

void ConsoleDiscoveryService::sendProbes() {
    QHostAddress broadcastAddr(QHostAddress::Broadcast);

    for (const auto& strategy : m_strategies) {
        QByteArray probeMsg = buildOscMessage(strategy->probeCommand());
        int port = strategy->probePort();

        // send to broadcast address
        m_socket.writeDatagram(probeMsg, broadcastAddr, port);

        // also send to all local subnet broadcast addresses
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            if (!(iface.flags() & QNetworkInterface::IsUp) ||
                !(iface.flags() & QNetworkInterface::IsRunning) ||
                (iface.flags() & QNetworkInterface::IsLoopBack)) {
                continue;
            }

            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QHostAddress subnetBroadcast = entry.broadcast();
                    if (!subnetBroadcast.isNull() && subnetBroadcast != broadcastAddr) {
                        m_socket.writeDatagram(probeMsg, subnetBroadcast, port);
                    }
                }
            }
        }
    }
}

void ConsoleDiscoveryService::onReadyRead() {
    while (m_socket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket.receiveDatagram();
        parseOscMessage(datagram.data(), datagram.senderAddress(), datagram.senderPort());
    }
}

void ConsoleDiscoveryService::onScanTimeout() {
    stopScan();
    emit scanFinished();
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
    QVariant value;

    if (typeStart < data.size() && data.at(typeStart) == ',') {
        int typeEnd = data.indexOf('\0', typeStart);
        if (typeEnd > typeStart) {
            QString types = QString::fromUtf8(data.mid(typeStart + 1, typeEnd - typeStart - 1));
            int argOffset = ((typeEnd + 4) / 4) * 4;

            if (!types.isEmpty() && argOffset <= data.size()) {
                value = parseOscArgument(data, argOffset, types.at(0).toLatin1());
            }
        }
    }

    // try each strategy to parse the response
    for (const auto& strategy : m_strategies) {
        if (strategy->canParseResponse(path)) {
            DiscoveredConsole console = strategy->parseResponse(path, value, sender, senderPort);

            if (console.isValid()) {
                // check if already discovered
                bool alreadyFound = false;
                for (const auto& existing : m_discovered) {
                    if (existing == console) {
                        alreadyFound = true;
                        break;
                    }
                }

                if (!alreadyFound) {
                    m_discovered.append(console);
                    emit consoleDiscovered(console);
                }
            }
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
        if (strEnd > offset) {
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
            if (offset + size <= data.size()) {
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
