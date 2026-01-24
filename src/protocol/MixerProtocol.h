#pragma once

#include "MixerCapabilities.h"
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>

namespace OpenMix {

class Cue;

enum class ConnectionState { Disconnected, Connecting, Connected, Reconnecting };

using ParameterCallback =
    std::function<void(const QString& path, const QVariant& value, bool success)>;

class MixerProtocol : public QObject {
    Q_OBJECT

  public:
    explicit MixerProtocol(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MixerProtocol() = default;

    virtual QString protocolName() const = 0;
    virtual QString protocolDescription() const = 0;

    virtual bool connect(const QString& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual QString connectionStatus() const = 0;
    virtual ConnectionState connectionState() const = 0;

    virtual void sendParameter(const QString& path, const QVariant& value) = 0;
    virtual QVariant getParameter(const QString& path) = 0;
    virtual void requestParameter(const QString& path) = 0;
    virtual void requestParameterAsync(const QString& path, ParameterCallback callback) = 0;

    virtual void recallSnapshot(const Cue& cue) = 0;
    virtual void recallScene(int sceneNumber) = 0;
    virtual void refresh() = 0;
    virtual int latencyMs() const = 0;
    virtual const MixerCapabilities& capabilities() const;

  signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void connectionStatusChanged(const QString& status);
    void connectionStateChanged(ConnectionState state);
    void connectionLost();
    void parameterChanged(const QString& path, const QVariant& value);
    void requestTimeout(const QString& path);
    void latencyChanged(int ms);
    void sceneChanged(int sceneNumber);
};

MixerProtocol* createMixerProtocol(const QString& type, QObject* parent = nullptr);

} // namespace OpenMix
