#include "ConnectionLogBridge.h"
#include "AppLogger.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <QJsonObject>

namespace OpenMix {

ConnectionLogBridge::ConnectionLogBridge(AppLogger* logger, QObject* parent)
    : QObject(parent), m_logger(logger) {}

ConnectionLogBridge::~ConnectionLogBridge() { detachFromMixer(); }

void ConnectionLogBridge::attachToMixer(MixerProtocol* mixer) {
    if (m_mixer == mixer) {
        return;
    }

    detachFromMixer();

    if (!mixer) {
        return;
    }

    m_mixer = mixer;

    connect(m_mixer, &MixerProtocol::connected, this, &ConnectionLogBridge::onConnected);
    connect(m_mixer, &MixerProtocol::disconnected, this, &ConnectionLogBridge::onDisconnected);
    connect(m_mixer, &MixerProtocol::connectionError, this,
            &ConnectionLogBridge::onConnectionError);
    connect(m_mixer, &MixerProtocol::connectionStateChanged, this,
            &ConnectionLogBridge::onConnectionStateChanged);
    connect(m_mixer, &MixerProtocol::connectionLost, this, &ConnectionLogBridge::onConnectionLost);
    connect(m_mixer, &MixerProtocol::latencyChanged, this, &ConnectionLogBridge::onLatencyChanged);
    connect(m_mixer, &MixerProtocol::requestTimeout, this, &ConnectionLogBridge::onRequestTimeout);
}

void ConnectionLogBridge::detachFromMixer() {
    if (!m_mixer) {
        return;
    }

    disconnect(m_mixer, nullptr, this, nullptr);
    m_mixer = nullptr;
}

void ConnectionLogBridge::setConnectionContext(const QString& protocol, const QString& host,
                                               int port) {
    m_protocol = protocol;
    m_host = host;
    m_port = port;
}

void ConnectionLogBridge::onConnected() {
    if (m_logger) {
        m_logger->logConnectionSuccess(m_protocol, m_host, m_port);
    }
}

void ConnectionLogBridge::onDisconnected() {
    if (m_logger) {
        m_logger->logDisconnected(m_protocol, m_host, m_port);
    }
}

void ConnectionLogBridge::onConnectionError(const QString& error) {
    if (m_logger) {
        m_logger->logConnectionFailed(m_protocol, m_host, m_port, error);
    }
}

void ConnectionLogBridge::onConnectionStateChanged(ConnectionState state) {
    if (!m_logger) {
        return;
    }

    QString stateStr;
    switch (state) {
    case ConnectionState::Disconnected:
        stateStr = "Disconnected";
        break;
    case ConnectionState::Connecting:
        stateStr = "Connecting";
        break;
    case ConnectionState::Connected:
        stateStr = "Connected";
        break;
    case ConnectionState::Reconnecting:
        stateStr = "Reconnecting";
        break;
    }

    QJsonObject meta;
    meta["protocol"] = m_protocol;
    meta["host"] = m_host;
    meta["port"] = m_port;
    meta["state"] = stateStr;

    m_logger->debug(LogSource::Connection, QString("Connection state changed: %1").arg(stateStr),
                    meta);
}

void ConnectionLogBridge::onConnectionLost() {
    if (m_logger) {
        m_logger->logConnectionLost(m_protocol, m_host, m_port);
    }
}

void ConnectionLogBridge::onLatencyChanged(int ms) {
    if (!m_logger) {
        return;
    }

    // throttle latency logging to avoid flooding
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastLatencyLogTime < LATENCY_LOG_THROTTLE_MS) {
        return;
    }
    m_lastLatencyLogTime = now;

    QJsonObject meta;
    meta["protocol"] = m_protocol;
    meta["host"] = m_host;
    meta["port"] = m_port;
    meta["latencyMs"] = ms;

    m_logger->debug(LogSource::Connection, QString("Latency: %1ms").arg(ms), meta);
}

void ConnectionLogBridge::onRequestTimeout(const QString& path) {
    if (!m_logger) {
        return;
    }

    QJsonObject meta;
    meta["protocol"] = m_protocol;
    meta["host"] = m_host;
    meta["port"] = m_port;
    meta["path"] = path;

    m_logger->warning(LogSource::Connection, QString("Request timeout: %1").arg(path), meta);
}

} // namespace OpenMix
