#pragma once

#include "DiscoveredConsole.h"
#include "OscProbeStrategy.h"
#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>
#include <memory>

namespace OpenMix {

class ConsoleDiscoveryService : public QObject {
    Q_OBJECT

  public:
    explicit ConsoleDiscoveryService(QObject* parent = nullptr);
    ~ConsoleDiscoveryService() override;

    void registerStrategy(OscProbeStrategyPtr strategy);

    void startScan(int timeoutMs = 3000);
    void stopScan();

    bool isScanning() const { return m_scanning; }
    const QVector<DiscoveredConsole>& discoveredConsoles() const { return m_discovered; }

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
    void parseOscMessage(const QByteArray& data, const QHostAddress& sender, int senderPort);
    QVariant parseOscArgument(const QByteArray& data, int& offset, char type);
    QByteArray buildOscMessage(const QString& path);

    QVector<OscProbeStrategyPtr> m_strategies;
    QVector<DiscoveredConsole> m_discovered;

    QUdpSocket m_socket;
    QTimer m_scanTimer;
    bool m_scanning = false;
};

} // namespace OpenMix
