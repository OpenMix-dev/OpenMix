#pragma once

#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QVariant>
#include <lo/lo.h>

namespace OpenMix {

class OscTransport : public QObject {
    Q_OBJECT

  public:
    explicit OscTransport(QObject* parent = nullptr);
    ~OscTransport() override;

    [[nodiscard]] bool connect(const QString& host, int port);
    void disconnect();
    [[nodiscard]] bool isConnected() const noexcept { return m_connected; }

    void send(const QString& path);
    void send(const QString& path, float value);
    void send(const QString& path, int value);
    void send(const QString& path, const QString& value);
    void send(const QString& path, const QVariant& value);

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] int port() const noexcept { return m_port; }

  signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void messageReceived(const QString& path, const QVariant& value);
    void rawMessageReceived(const QByteArray& data);

  private slots:
    void onReadyRead();

  private:
    void sendMessage(const QString& path, lo_message msg);
    void parseOscMessage(const QByteArray& data);
    QVariant parseOscArgument(const QByteArray& data, int& offset, char type);

    QUdpSocket m_socket;
    QHostAddress m_target;
    QString m_host;
    int m_port = 0;
    bool m_connected = false;
};

} // namespace OpenMix
