#pragma once

#include "protocol/MixerProtocol.h"
#include "protocol/discovery/DiscoveredConsole.h"
#include <QAction>
#include <QWidget>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;

namespace OpenMix {

class Application;
class ConnectionStateWidget;
class ConsoleDiscoveryWidget;

class ConnectionPanel : public QWidget {
    Q_OBJECT

  public:
    explicit ConnectionPanel(Application* app, QWidget* parent = nullptr);

  private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshClicked();
    void onConnectionStatusChanged(const QString& status);
    void onConnectionStateChanged(ConnectionState state);
    void onLatencyChanged(int ms);
    void onRequestTimeout(const QString& path);
    void onConnectionError(const QString& error);
    void onConnected();
    void onDisconnected();
    void onProtocolTypeChanged(int index);
    void onDiscoveredConsoleSelected(const DiscoveredConsole& console);
    void onDiscoveredConsoleDoubleClicked(const DiscoveredConsole& console);

  private:
    void setupUi();
    void populateProtocolCombo();
    void updateUiState();
    void loadFromConfig();
    void saveToConfig();
    void connectMixerSignals(MixerProtocol* mixer);
    // pull the driver's current state/status into the widgets (covers anything
    // it emitted before we were listening, and a driver that failed to exist)
    void syncFromMixer();
    void showDisconnected();

    Application* m_app;

    ConsoleDiscoveryWidget* m_discoveryWidget;
    QComboBox* m_protocolCombo;
    QLineEdit* m_hostEdit;
    QLineEdit* m_portEdit;
    QLabel* m_hostLabel;
    QLabel* m_portLabel;
    QComboBox* m_faderLawCombo;
    QLabel* m_faderLawLabel;
    QSpinBox* m_midiChannelSpin;
    QLabel* m_midiChannelLabel;
    QLineEdit* m_oscFaderEdit;
    QLineEdit* m_oscMuteEdit;
    QLineEdit* m_oscSceneEdit;
    QLabel* m_oscFaderLabel;
    QLabel* m_oscMuteLabel;
    QLabel* m_oscSceneLabel;
    QLabel* m_oscHintLabel;
    QLabel* m_loopbackLabel;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    QLabel* m_latencyLabel;
    ConnectionStateWidget* m_stateWidget;

    // actions
    QAction* m_connectAction;
    QAction* m_disconnectAction;
    QAction* m_refreshDiscoveryAction;

    int m_timeoutCount = 0;
    ConnectionState m_currentState = ConnectionState::Disconnected;
    // the last failure the driver reported; shown with the Disconnected state so
    // a refused or timed-out connect does not just read "Disconnected"
    QString m_lastError;
};

} // namespace OpenMix
