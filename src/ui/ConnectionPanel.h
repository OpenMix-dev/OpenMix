#pragma once

#include "protocol/MixerProtocol.h"
#include <QWidget>

class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

namespace OpenMix {

class Application;
class ConnectionStateWidget;

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
    void onConnected();
    void onDisconnected();
    void onProtocolTypeChanged(int index);

  private:
    void setupUi();
    void updateUiState();
    void loadFromConfig();
    void saveToConfig();
    void connectMixerSignals();

    Application* m_app;

    QComboBox* m_protocolCombo;
    QLineEdit* m_hostEdit;
    QLineEdit* m_portEdit;
    QLabel* m_hostLabel;
    QLabel* m_portLabel;
    QLabel* m_loopbackLabel;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    QLabel* m_latencyLabel;
    ConnectionStateWidget* m_stateWidget;

    int m_timeoutCount = 0;
};

} // namespace OpenMix
