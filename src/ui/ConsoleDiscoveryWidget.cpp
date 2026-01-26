#include "ConsoleDiscoveryWidget.h"
#include "protocol/discovery/ConsoleDiscoveryService.h"
#include "theme/Icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace OpenMix {

ConsoleDiscoveryWidget::ConsoleDiscoveryWidget(ConsoleDiscoveryService* service, QWidget* parent)
    : QWidget(parent), m_service(service) {
    setupUi();

    connect(m_service, &ConsoleDiscoveryService::scanStarted, this,
            &ConsoleDiscoveryWidget::onScanStarted);
    connect(m_service, &ConsoleDiscoveryService::scanFinished, this,
            &ConsoleDiscoveryWidget::onScanFinished);
    connect(m_service, &ConsoleDiscoveryService::scanError, this,
            &ConsoleDiscoveryWidget::onScanError);
    connect(m_service, &ConsoleDiscoveryService::consoleDiscovered, this,
            &ConsoleDiscoveryWidget::onConsoleDiscovered);

    updateUiState();
}

void ConsoleDiscoveryWidget::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // scan button and status row
    QHBoxLayout* headerLayout = new QHBoxLayout();

    m_scanButton = new QPushButton(Icons::network(), tr("Scan Network"), this);
    m_scanButton->setToolTip(tr("Scan for OSC-enabled mixing consoles on the network"));
    connect(m_scanButton, &QPushButton::clicked, this, &ConsoleDiscoveryWidget::onScanClicked);
    headerLayout->addWidget(m_scanButton);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // indeterminate
    m_progressBar->setMaximumWidth(100);
    m_progressBar->setVisible(false);
    headerLayout->addWidget(m_progressBar);

    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // console list
    m_consoleList = new QListWidget(this);
    m_consoleList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_consoleList->setMinimumHeight(80);
    m_consoleList->setMaximumHeight(150);
    connect(m_consoleList, &QListWidget::itemSelectionChanged, this,
            &ConsoleDiscoveryWidget::onItemSelectionChanged);
    connect(m_consoleList, &QListWidget::itemDoubleClicked, this,
            &ConsoleDiscoveryWidget::onItemDoubleClicked);
    layout->addWidget(m_consoleList);

    // status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray; font-style: italic;");
    m_statusLabel->setText(tr("Click 'Scan Network' to find mixers"));
    layout->addWidget(m_statusLabel);
}

DiscoveredConsole ConsoleDiscoveryWidget::selectedConsole() const {
    int row = m_consoleList->currentRow();
    if (row >= 0 && row < m_discoveredConsoles.size()) {
        return m_discoveredConsoles.at(row);
    }
    return DiscoveredConsole();
}

void ConsoleDiscoveryWidget::onScanClicked() {
    m_discoveredConsoles.clear();
    m_consoleList->clear();
    m_service->startScan(3000);
}

void ConsoleDiscoveryWidget::onScanStarted() {
    m_statusLabel->setText(tr("Scanning for mixers..."));
    m_progressBar->setVisible(true);
    m_scanButton->setEnabled(false);
    updateUiState();
}

void ConsoleDiscoveryWidget::onScanFinished() {
    m_progressBar->setVisible(false);
    m_scanButton->setEnabled(true);

    int count = m_discoveredConsoles.size();
    if (count == 0) {
        m_statusLabel->setText(tr("No OSC-enabled mixers found"));
    } else if (count == 1) {
        m_statusLabel->setText(tr("Found 1 mixer"));
        m_consoleList->setCurrentRow(0);
    } else {
        m_statusLabel->setText(tr("Found %1 mixers").arg(count));
    }

    updateUiState();
}

void ConsoleDiscoveryWidget::onScanError(const QString& error) {
    m_progressBar->setVisible(false);
    m_scanButton->setEnabled(true);
    m_statusLabel->setText(tr("Scan error: %1").arg(error));
    updateUiState();
}

void ConsoleDiscoveryWidget::onConsoleDiscovered(const DiscoveredConsole& console) {
    m_discoveredConsoles.append(console);

    QString displayText = console.toString();
    QListWidgetItem* item = new QListWidgetItem(displayText, m_consoleList);

    // set icon based on manufacturer
    switch (console.manufacturer) {
    case Manufacturer::Behringer:
    case Manufacturer::Midas:
        item->setIcon(Icons::sliders());
        break;
    case Manufacturer::Yamaha:
        item->setIcon(Icons::sliders());
        break;
    default:
        item->setIcon(Icons::helpAbout());
        break;
    }

    m_statusLabel->setText(tr("Found %1 mixer(s)...").arg(m_discoveredConsoles.size()));
}

void ConsoleDiscoveryWidget::onItemSelectionChanged() {
    DiscoveredConsole console = selectedConsole();
    if (console.isValid()) {
        emit consoleSelected(console);
    }
    updateUiState();
}

void ConsoleDiscoveryWidget::onItemDoubleClicked(QListWidgetItem* item) {
    Q_UNUSED(item);
    DiscoveredConsole console = selectedConsole();
    if (console.isValid()) {
        emit consoleDoubleClicked(console);
    }
}

void ConsoleDiscoveryWidget::updateUiState() {
    bool scanning = m_service->isScanning();
    m_scanButton->setEnabled(!scanning);
    m_consoleList->setEnabled(!scanning);
}

} // namespace OpenMix
