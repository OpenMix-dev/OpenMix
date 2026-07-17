#include "DiGiCoProtocol.h"
#include "../../core/Cue.h"
#include "../../core/LevelDb.h"

namespace OpenMix {

DiGiCoProtocol::DiGiCoProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {
    m_port = caps.defaultPort;
}

DiGiCoProtocol::~DiGiCoProtocol() { disconnect(); }

QString DiGiCoProtocol::expand(const QString& pattern, int channel) {
    if (pattern.isEmpty()) {
        return {};
    }
    QString address = pattern;
    return address.replace(QLatin1Char('*'), QString::number(channel));
}

bool DiGiCoProtocol::connect(const QString& host, int port) {
    disconnect();

    m_host = host;
    m_port = port;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(port));

    if (!m_transport.connect(host, port)) {
        setStatus("Failed to initialize transport");
        setConnectionState(ConnectionState::Disconnected);
        emit connectionError("Failed to initialize transport");
        return false;
    }

    // Generic OSC answers nothing on its own, so there is no handshake to wait
    // for and no way to tell a listening console from a switched-off one. The
    // link is "up" once the socket is, and the console only acts on any of this
    // with External Control enabled.
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Sending to %1:%2 (no reply expected)").arg(host).arg(port));
    emit connected();
    return true;
}

void DiGiCoProtocol::disconnect() {
    m_transport.disconnect();
    m_parameterCache.clear();
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void DiGiCoProtocol::setChannelFaderDb(int channel, double dB) {
    const QString address = expand(m_templates.channelFader, channel);
    if (!isConnected() || address.isEmpty()) {
        return; // no pattern: the console was never told how to do this
    }
    m_transport.send(address, dB <= NEG_INF_DB ? -128.0f : static_cast<float>(dB));
    m_parameterCache[address] = dB;
}

void DiGiCoProtocol::setChannelMute(int channel, bool muted) {
    const QString address = expand(m_templates.channelMute, channel);
    if (!isConnected() || address.isEmpty()) {
        return;
    }
    m_transport.send(address, muted ? 1 : 0);
    m_parameterCache[address] = muted;
}

void DiGiCoProtocol::recallScene(int sceneNumber) {
    if (!isConnected() || m_templates.sceneRecall.isEmpty() || sceneNumber < 1) {
        return;
    }
    // the scene number goes in a "*" if the pattern has one, and as the argument
    // otherwise: consoles are configured both ways
    const QString address = expand(m_templates.sceneRecall, sceneNumber);
    if (m_templates.sceneRecall.contains(QLatin1Char('*'))) {
        m_transport.send(address);
    } else {
        m_transport.send(address, sceneNumber);
    }
}

void DiGiCoProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (!isConnected()) {
        return;
    }

    const QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 3 || parts[0] != "ch") {
        return;
    }
    bool ok = false;
    const int channel = parts[1].toInt(&ok);
    if (!ok || channel < 1) {
        return;
    }

    if (parts[2] == "fader") {
        setChannelFaderDb(channel, value.toDouble());
    } else if (parts[2] == "mute") {
        setChannelMute(channel, value.toBool());
    }
}

QVariant DiGiCoProtocol::getParameter(const QString& path) { return m_parameterCache.value(path); }

void DiGiCoProtocol::requestParameter(const QString& path) { Q_UNUSED(path); }

void DiGiCoProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (callback) {
        callback(path, QVariant(), false);
    }
}

void DiGiCoProtocol::recallSnapshot(const Cue& cue) {
    if (!isConnected()) {
        return;
    }
    const QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void DiGiCoProtocol::refresh() {}

void DiGiCoProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void DiGiCoProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
