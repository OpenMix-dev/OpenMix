#include "ConnectionPanel.h"
#include "ConnectionStateWidget.h"
#include "app/Application.h"
#include "core/Show.h"
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

namespace StageBlend {

ConnectionPanel::ConnectionPanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    loadFromConfig();
    updateUiState();
}

void ConnectionPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // protocol selection
    QGroupBox* connectionGroup = new QGroupBox(tr("Mixer Connection"), this);
    QFormLayout* formLayout = new QFormLayout(connectionGroup);

    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItem(tr("Behringer X32 / Midas M32"), "x32");
    m_protocolCombo->addItem(tr("Yamaha CL/QL/TF/DM (Coming Soon)"), "yamaha");
    m_protocolCombo->addItem(tr("Allen & Heath (Coming Soon)"), "allen-heath");
    // disable unsupported protocols
    m_protocolCombo->model()->setData(m_protocolCombo->model()->index(1, 0), false,
                                      Qt::UserRole - 1);
    m_protocolCombo->model()->setData(m_protocolCombo->model()->index(2, 0), false,
                                      Qt::UserRole - 1);
    formLayout->addRow(tr("Protocol:"), m_protocolCombo);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(tr("192.168.1.1"));
    formLayout->addRow(tr("IP Address:"), m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(10023); // default X32 port
    formLayout->addRow(tr("Port:"), m_portSpin);

    mainLayout->addWidget(connectionGroup);

    // buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Request all parameters from mixer"));

    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    buttonLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(buttonLayout);

    // status
    QGroupBox* statusGroup = new QGroupBox(tr("Status"), this);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    // state indicator row
    QHBoxLayout* stateRow = new QHBoxLayout();
    m_stateWidget = new ConnectionStateWidget(this);
    stateRow->addWidget(m_stateWidget);

    m_statusLabel = new QLabel(tr("Disconnected"), this);
    m_statusLabel->setWordWrap(true);
    stateRow->addWidget(m_statusLabel, 1);
    statusLayout->addLayout(stateRow);

    // latency display
    m_latencyLabel = new QLabel(this);
    m_latencyLabel->setVisible(false);
    statusLayout->addWidget(m_latencyLabel);

    mainLayout->addWidget(statusGroup);

    mainLayout->addStretch();

    // connections
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

    if (host.isEmpty()) {
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
    m_statusLabel->setText(
        tr("Connected to %1:%2").arg(m_hostEdit->text()).arg(m_portSpin->value()));
    m_stateWidget->setState(ConnectionState::Connected);
    m_latencyLabel->setVisible(true);
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
    // update default port based on protocol
    QString type = m_protocolCombo->itemData(index).toString();
    if (type == "x32") {
        m_portSpin->setValue(10023);
    } else if (type == "yamaha") {
        m_portSpin->setValue(49280); // yamaha default
    }
}

void ConnectionPanel::updateUiState() {
    MixerProtocol* mixer = m_app->mixer();
    bool connected = mixer && mixer->isConnected();

    m_protocolCombo->setEnabled(!connected);
    m_hostEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_refreshButton->setEnabled(connected);
}

void ConnectionPanel::loadFromConfig() {
    MixerConfig config = m_app->show()->mixerConfig();

    // set protocol
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

} // namespace StageBlend
