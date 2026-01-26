#pragma once

#include "protocol/discovery/DiscoveredConsole.h"
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QProgressBar;

namespace OpenMix {

class ConsoleDiscoveryService;

class ConsoleDiscoveryWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ConsoleDiscoveryWidget(ConsoleDiscoveryService* service, QWidget* parent = nullptr);

    DiscoveredConsole selectedConsole() const;

  signals:
    void consoleSelected(const DiscoveredConsole& console);
    void consoleDoubleClicked(const DiscoveredConsole& console);

  private slots:
    void onScanClicked();
    void onScanStarted();
    void onScanFinished();
    void onScanError(const QString& error);
    void onConsoleDiscovered(const DiscoveredConsole& console);
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);

  private:
    void setupUi();
    void updateUiState();

    ConsoleDiscoveryService* m_service;

    QPushButton* m_scanButton;
    QListWidget* m_consoleList;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    QVector<DiscoveredConsole> m_discoveredConsoles;
};

} // namespace OpenMix
