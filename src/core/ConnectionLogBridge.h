#pragma once

#include <QObject>
#include <QString>

namespace OpenMix {

class AppLogger;
class MixerProtocol;
enum class ConnectionState;

class ConnectionLogBridge : public QObject {
    Q_OBJECT

  public:
    explicit ConnectionLogBridge(AppLogger* logger, QObject* parent = nullptr);
    ~ConnectionLogBridge() override;

    void attachToMixer(MixerProtocol* mixer);
    void detachFromMixer();

    void setConnectionContext(const QString& protocol, const QString& host, int port);

    MixerProtocol* attachedMixer() const { return m_mixer; }

  private slots:
    void onConnected();
    void onDisconnected();
    void onConnectionError(const QString& error);
    void onConnectionStateChanged(ConnectionState state);
    void onConnectionLost();
    void onLatencyChanged(int ms);
    void onRequestTimeout(const QString& path);

  private:
    AppLogger* m_logger;
    MixerProtocol* m_mixer = nullptr;

    QString m_protocol;
    QString m_host;
    int m_port = 0;

    // throttle latency logging
    qint64 m_lastLatencyLogTime = 0;
    static constexpr int LATENCY_LOG_THROTTLE_MS = 5000;
};

} // namespace OpenMix
