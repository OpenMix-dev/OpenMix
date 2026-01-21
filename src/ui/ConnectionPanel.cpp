#include "ConnectionPanel.h"
#include "ConnectionStateWidget.h"
#include "app/Application.h"
#include "core/Show.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace OpenMix {

ConnectionPanel::ConnectionPanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    loadFromConfig();
    onProtocolTypeChanged(m_protocolCombo->currentIndex());
    updateUiState();
}

void ConnectionPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* connectionGroup = new QGroupBox(tr("Mixer Connection"), this);
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
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(10023); // default X32 port
    formLayout->addRow(m_portLabel, m_portSpin);

    m_loopbackLabel = new QLabel(tr("No hardware connection required."), this);
    m_loopbackLabel->setStyleSheet("color: gray; font-style: italic;");
    m_loopbackLabel->setVisible(false);
    formLayout->addRow(QString(), m_loopbackLabel);

    mainLayout->addWidget(connectionGroup);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    m_refreshButton = new QPushButton(tr("Refresh"), this);
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
    int port = m_portSpin->value();

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
            tr("Connected to %1:%2").arg(m_hostEdit->text()).arg(m_portSpin->value()));
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
            tr("Connected to %1:%2").arg(m_hostEdit->text()).arg(m_portSpin->value()));
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
    m_portSpin->setVisible(!isLoopback);
    m_loopbackLabel->setVisible(isLoopback);

    if (!isLoopback) {
        m_portSpin->setValue(caps.defaultPort);
    }
}

void ConnectionPanel::updateUiState() {
    MixerProtocol* mixer = m_app->mixer();
    bool connected = mixer && mixer->isConnected();

    QString type = m_protocolCombo->currentData().toString();
    MixerCapabilities caps = MixerCapabilities::forProtocolId(type);
    bool isLoopback = (caps.protocol == ProtocolType::Internal);

    m_protocolCombo->setEnabled(!connected);
    m_hostEdit->setEnabled(!connected && !isLoopback);
    m_portSpin->setEnabled(!connected && !isLoopback);
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
    m_portSpin->setValue(config.port);
}

void ConnectionPanel::saveToConfig() {
    MixerConfig config;
    config.type = m_protocolCombo->currentData().toString();
    config.host = m_hostEdit->text().trimmed();
    config.port = m_portSpin->value();
    m_app->show()->setMixerConfig(config);
}

} // namespace OpenMix
