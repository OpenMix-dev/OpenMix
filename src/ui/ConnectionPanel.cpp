#include "ConnectionPanel.h"
#include "ConnectionStateWidget.h"
#include "ConsoleDiscoveryWidget.h"
#include "app/Application.h"
#include "core/Show.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include "protocol/discovery/ConsoleDiscoveryService.h"
#include "theme/Icons.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace OpenMix {

ConnectionPanel::ConnectionPanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    loadFromConfig();
    onProtocolTypeChanged(m_protocolCombo->currentIndex());
    updateUiState();
}

void ConnectionPanel::setupUi() {
    setMinimumSize(240, 600);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // auto-discovery section
    QGroupBox* discoveryGroup = new QGroupBox(tr("Auto-Discovery"), this);
    QVBoxLayout* discoveryLayout = new QVBoxLayout(discoveryGroup);

    m_discoveryWidget = new ConsoleDiscoveryWidget(m_app->discoveryService(), this);
    connect(m_discoveryWidget, &ConsoleDiscoveryWidget::consoleSelected, this,
            &ConnectionPanel::onDiscoveredConsoleSelected);
    connect(m_discoveryWidget, &ConsoleDiscoveryWidget::consoleDoubleClicked, this,
            &ConnectionPanel::onDiscoveredConsoleDoubleClicked);
    discoveryLayout->addWidget(m_discoveryWidget);

    mainLayout->addWidget(discoveryGroup);

    // manual connection section
    QGroupBox* connectionGroup = new QGroupBox(tr("Manual Connection"), this);
    QFormLayout* formLayout = new QFormLayout(connectionGroup);

    m_protocolCombo = new QComboBox(this);

    m_protocolCombo->addItem(tr("Loopback (Test)"), "loopback");
    m_protocolCombo->addItem(tr("Allen & Heath Avantis"), "avantis");
    m_protocolCombo->addItem(tr("Allen & Heath dLive"), "dlive");
    m_protocolCombo->addItem(tr("Allen & Heath GLD-80"), "gld80");
    m_protocolCombo->addItem(tr("Allen & Heath GLD-112"), "gld112");
    m_protocolCombo->addItem(tr("Allen & Heath SQ-5"), "sq5");
    m_protocolCombo->addItem(tr("Allen & Heath SQ-6"), "sq6");
    m_protocolCombo->addItem(tr("Allen & Heath SQ-7"), "sq7");
    m_protocolCombo->addItem(tr("Behringer WING"), "wing");
    m_protocolCombo->addItem(tr("Behringer X32"), "x32");
    m_protocolCombo->addItem(tr("Midas M32"), "m32");
    m_protocolCombo->addItem(tr("Yamaha CL1"), "cl1");
    m_protocolCombo->addItem(tr("Yamaha CL3"), "cl3");
    m_protocolCombo->addItem(tr("Yamaha CL5"), "cl5");
    m_protocolCombo->addItem(tr("Yamaha DM7"), "dm7");
    m_protocolCombo->addItem(tr("Yamaha QL1"), "ql1");
    m_protocolCombo->addItem(tr("Yamaha QL5"), "ql5");
    m_protocolCombo->addItem(tr("Yamaha TF1"), "tf1");
    m_protocolCombo->addItem(tr("Yamaha TF3"), "tf3");
    m_protocolCombo->addItem(tr("Yamaha TF5"), "tf5");

    formLayout->addRow(tr("Protocol:"), m_protocolCombo);

    m_hostLabel = new QLabel(tr("IP Address:"), this);
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(tr("192.168.1.1"));
    formLayout->addRow(m_hostLabel, m_hostEdit);

    m_portLabel = new QLabel(tr("Port:"), this);
    m_portEdit = new QLineEdit(this);
    m_portEdit->setPlaceholderText(tr("10023")); // default X32 port
    formLayout->addRow(m_portLabel, m_portEdit);

    m_loopbackLabel = new QLabel(tr("No hardware connection required."), this);
    m_loopbackLabel->setStyleSheet("color: gray; font-style: italic;");
    m_loopbackLabel->setVisible(false);
    formLayout->addRow(QString(), m_loopbackLabel);

    mainLayout->addWidget(connectionGroup);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton(Icons::network(), tr("Connect"), this);
    m_connectButton->setToolTip(tr("Connect to the mixer"));
    m_disconnectButton = new QPushButton(Icons::disconnect(), tr("Disconnect"), this);
    m_disconnectButton->setToolTip(tr("Disconnect from the mixer"));
    m_refreshButton = new QPushButton(Icons::refresh(), tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Request all parameters from mixer"));

    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    buttonLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(buttonLayout);

    QGroupBox* statusGroup = new QGroupBox(tr("Status"), this);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    QHBoxLayout* stateRow = new QHBoxLayout();
    m_stateWidget = new ConnectionStateWidget(this);
    stateRow->addWidget(m_stateWidget);

    m_statusLabel = new QLabel(tr("Disconnected"), this);
    m_statusLabel->setWordWrap(true);
    stateRow->addWidget(m_statusLabel, 1);
    statusLayout->addLayout(stateRow);

    m_latencyLabel = new QLabel(this);
    m_latencyLabel->setVisible(false);
    statusLayout->addWidget(m_latencyLabel);

    mainLayout->addWidget(statusGroup);

    mainLayout->addStretch();

    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ConnectionPanel::onProtocolTypeChanged);
    connect(m_connectButton, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &ConnectionPanel::onDisconnectClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &ConnectionPanel::onRefreshClicked);
}

void ConnectionPanel::onConnectClicked() {
    saveToConfig();

    QString type = m_protocolCombo->currentData().toString();
    QString host = m_hostEdit->text().trimmed();
    int port = m_portEdit->text().toInt();

    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);
    bool isLoopback = (caps.protocol == ProtocolType::Internal);

    if (!isLoopback && host.isEmpty()) {
        m_statusLabel->setText(tr("Please enter an IP address"));
        return;
    }

    m_statusLabel->setText(tr("Connecting..."));
    m_stateWidget->setState(ConnectionState::Connecting);
    m_timeoutCount = 0;

    m_app->connectToMixer(type, host, port);
    connectMixerSignals();
    updateUiState();
}

void ConnectionPanel::connectMixerSignals() {
    MixerProtocol* mixer = m_app->mixer();
    if (!mixer)
        return;

    connect(mixer, &MixerProtocol::connectionStatusChanged, this,
            &ConnectionPanel::onConnectionStatusChanged, Qt::UniqueConnection);
    connect(mixer, &MixerProtocol::connectionStateChanged, this,
            &ConnectionPanel::onConnectionStateChanged, Qt::UniqueConnection);
    connect(mixer, &MixerProtocol::latencyChanged, this, &ConnectionPanel::onLatencyChanged,
            Qt::UniqueConnection);
    connect(mixer, &MixerProtocol::requestTimeout, this, &ConnectionPanel::onRequestTimeout,
            Qt::UniqueConnection);
    connect(mixer, &MixerProtocol::connected, this, &ConnectionPanel::onConnected,
            Qt::UniqueConnection);
    connect(mixer, &MixerProtocol::disconnected, this, &ConnectionPanel::onDisconnected,
            Qt::UniqueConnection);
}

void ConnectionPanel::onDisconnectClicked() {
    m_app->disconnectFromMixer();
    m_statusLabel->setText(tr("Disconnected"));
    m_stateWidget->setState(ConnectionState::Disconnected);
    m_latencyLabel->setVisible(false);
    m_timeoutCount = 0;
    updateUiState();
}

void ConnectionPanel::onRefreshClicked() {
    MixerProtocol* mixer = m_app->mixer();
    if (mixer && mixer->isConnected()) {
        mixer->refresh();
        m_statusLabel->setText(tr("Refreshing parameters..."));
    }
}

void ConnectionPanel::onConnectionStatusChanged(const QString& status) {
    m_statusLabel->setText(status);
}

void ConnectionPanel::onConnectionStateChanged(ConnectionState state) {
    m_stateWidget->setState(state);

    switch (state) {
    case ConnectionState::Disconnected:
        m_statusLabel->setText(tr("Disconnected"));
        m_latencyLabel->setVisible(false);
        break;
    case ConnectionState::Connecting:
        m_statusLabel->setText(tr("Connecting..."));
        break;
    case ConnectionState::Connected:
        m_statusLabel->setText(
            tr("Connected to %1:%2").arg(m_hostEdit->text()).arg(m_portEdit->text()));
        m_latencyLabel->setVisible(true);
        m_timeoutCount = 0;
        break;
    case ConnectionState::Reconnecting:
        m_statusLabel->setText(tr("Connection lost, reconnecting..."));
        break;
    }

    updateUiState();
}

void ConnectionPanel::onLatencyChanged(int ms) {
    m_stateWidget->setLatency(ms);

    QString latencyText;
    QString colorStyle;

    if (ms < 50) {
        latencyText = tr("Latency: %1ms (Excellent)").arg(ms);
        colorStyle = "color: #00aa00;"; // green
    } else if (ms < 100) {
        latencyText = tr("Latency: %1ms (Good)").arg(ms);
        colorStyle = "color: #88aa00;"; // yellow-green
    } else if (ms < 200) {
        latencyText = tr("Latency: %1ms (Fair)").arg(ms);
        colorStyle = "color: #aaaa00;"; // yellow
    } else {
        latencyText = tr("Latency: %1ms (Poor)").arg(ms);
        colorStyle = "color: #aa5500;"; // orange
    }

    m_latencyLabel->setText(latencyText);
    m_latencyLabel->setStyleSheet(colorStyle);
}

void ConnectionPanel::onRequestTimeout(const QString& path) {
    m_timeoutCount++;

    if (m_timeoutCount == 1) {
        m_statusLabel->setText(tr("Warning: Request timeout (%1)").arg(path));
    } else {
        m_statusLabel->setText(tr("Warning: %1 request timeouts").arg(m_timeoutCount));
    }
}

void ConnectionPanel::onConnected() {
    MixerProtocol* mixer = m_app->mixer();

    // use mixer's status message if available
    if (mixer && !mixer->connectionStatus().isEmpty()) {
        m_statusLabel->setText(mixer->connectionStatus());
    } else {
        m_statusLabel->setText(
            tr("Connected to %1:%2").arg(m_hostEdit->text()).arg(m_portEdit->text()));
    }

    m_stateWidget->setState(ConnectionState::Connected);

    // hide latency for loopback (not meaningful)
    QString type = m_protocolCombo->currentData().toString();
    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);
    bool isLoopback = (caps.protocol == ProtocolType::Internal);
    m_latencyLabel->setVisible(!isLoopback);

    m_timeoutCount = 0;
    updateUiState();
}

void ConnectionPanel::onDisconnected() {
    m_statusLabel->setText(tr("Disconnected"));
    m_stateWidget->setState(ConnectionState::Disconnected);
    m_latencyLabel->setVisible(false);
    updateUiState();
}

void ConnectionPanel::onProtocolTypeChanged(int index) {
    QString type = m_protocolCombo->itemData(index).toString();
    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);

    bool isLoopback = (caps.protocol == ProtocolType::Internal);

    // show/hide network fields based on protocol type
    m_hostLabel->setVisible(!isLoopback);
    m_hostEdit->setVisible(!isLoopback);
    m_portLabel->setVisible(!isLoopback);
    m_portEdit->setVisible(!isLoopback);
    m_loopbackLabel->setVisible(isLoopback);

    if (!isLoopback) {
        m_portEdit->setText(QString::number(caps.defaultPort));
    }

    updateUiState();
}

void ConnectionPanel::onDiscoveredConsoleSelected(const DiscoveredConsole& console) {
    if (!console.isValid()) {
        return;
    }

    // populate connection fields from discovered console
    MixerCapabilities caps = console.toCapabilities();

    int idx = m_protocolCombo->findData(caps.protocolId);
    if (idx >= 0) {
        m_protocolCombo->setCurrentIndex(idx);
    }

    m_hostEdit->setText(console.address.toString());
    m_portEdit->setText(QString::number(console.port));

    m_statusLabel->setText(tr("Selected: %1").arg(console.toString()));
}

void ConnectionPanel::onDiscoveredConsoleDoubleClicked(const DiscoveredConsole& console) {
    if (!console.isValid()) {
        return;
    }

    // auto-connect to the discovered console
    onDiscoveredConsoleSelected(console);

    m_statusLabel->setText(tr("Connecting to %1...").arg(console.displayName));
    m_stateWidget->setState(ConnectionState::Connecting);
    m_timeoutCount = 0;

    m_app->connectToDiscoveredConsole(console);
    connectMixerSignals();
    updateUiState();
}

void ConnectionPanel::updateUiState() {
    MixerProtocol* mixer = m_app->mixer();
    bool connected = mixer && mixer->isConnected();

    QString type = m_protocolCombo->currentData().toString();
    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);
    bool isLoopback = (caps.protocol == ProtocolType::Internal);

    m_protocolCombo->setEnabled(!connected);
    m_hostEdit->setEnabled(!connected && !isLoopback);
    m_portEdit->setEnabled(!connected && !isLoopback);
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_refreshButton->setEnabled(connected);
}

void ConnectionPanel::loadFromConfig() {
    MixerConfig config = m_app->show()->mixerConfig();

    int idx = m_protocolCombo->findData(config.type);
    if (idx >= 0) {
        m_protocolCombo->setCurrentIndex(idx);
    }

    m_hostEdit->setText(config.host);
    m_portEdit->setText(QString::number(config.port));
}

void ConnectionPanel::saveToConfig() {
    MixerConfig config;
    config.type = m_protocolCombo->currentData().toString();
    config.host = m_hostEdit->text().trimmed();
    config.port = m_portEdit->text().toInt();
    m_app->show()->setMixerConfig(config);
}

} // namespace OpenMix
