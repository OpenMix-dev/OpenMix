#pragma once

#include "DiscoveredConsole.h"
#include "OscProbeStrategy.h"
#include <QHostAddress>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

class QTcpSocket;

namespace OpenMix {

class ConsoleDiscoveryService : public QObject {
    Q_OBJECT

  public:
    explicit ConsoleDiscoveryService(QObject* parent = nullptr);
    ~ConsoleDiscoveryService() override;

    void registerStrategy(OscProbeStrategyPtr strategy);

    void startScan(int timeoutMs = 3000);
    void stopScan();

    // unicast all probes to a known host (fallback for networks that drop broadcasts);
    // only effective while a scan is running
    void probeHost(const QHostAddress& host);

    bool isScanning() const { return m_scanning; }
    const QVector<DiscoveredConsole>& discoveredConsoles() const { return m_discovered; }

    // handles one inbound discovery datagram (exposed for tests)
    void processDatagram(const QByteArray& data, const QHostAddress& sender, int senderPort);

    static QByteArray buildOscMessage(const QString& path);

  signals:
    void scanStarted();
    void scanFinished();
    void scanError(const QString& error);
    void consoleDiscovered(const DiscoveredConsole& console);

  private slots:
    void onReadyRead();
    void onScanTimeout();

  private:
    void sendProbes();
    void sendProbesTo(const QHostAddress& target, const QHostAddress& localAddress);
    void addConsole(const DiscoveredConsole& console);
    void parseOscMessage(const QByteArray& data, const QHostAddress& sender, int senderPort);
    QVariant parseOscArgument(const QByteArray& data, int& offset, char type);

    // launches one async TCP handshake that identifies an Allen & Heath console
    // after it answers a UDP "Find"; result is emitted via consoleDiscovered
    void launchTcpIdentify(const OscProbeStrategyPtr& strategy, const QHostAddress& host,
                           const TcpIdentify& identify);
    void cancelIdentifyProbes();

    QVector<OscProbeStrategyPtr> m_strategies;
    QVector<DiscoveredConsole> m_discovered;

    QUdpSocket m_socket;
    QTimer m_scanTimer;
    bool m_scanning = false;

    // per-scan bookkeeping for the TCP identify stage
    QSet<QString> m_identifyProbed; // "ip:port" already probed, avoids duplicates
    QVector<QTcpSocket*> m_identifySockets;
};

} // namespace OpenMix
