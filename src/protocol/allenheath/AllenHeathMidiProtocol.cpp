#include "AllenHeathMidiProtocol.h"
#include "../../core/Cue.h"

namespace OpenMix {

AllenHeathMidiProtocol::AllenHeathMidiProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps), m_transport(this) {

    m_port = caps.defaultPort; // 51325

    QObject::connect(&m_transport, &TcpTransport::connected, this,
                     &AllenHeathMidiProtocol::onTransportConnected);
    QObject::connect(&m_transport, &TcpTransport::disconnected, this,
                     &AllenHeathMidiProtocol::onTransportDisconnected);
    QObject::connect(&m_transport, &TcpTransport::connectionError, this,
                     &AllenHeathMidiProtocol::onTransportError);
    QObject::connect(&m_transport, &TcpTransport::connectionLost, this,
                     &AllenHeathMidiProtocol::onTransportConnectionLost);
    QObject::connect(&m_transport, &TcpTransport::dataReceived, this,
                     &AllenHeathMidiProtocol::onDataReceived);
    QObject::connect(&m_transport, &TcpTransport::reconnecting, this,
                     &AllenHeathMidiProtocol::onReconnecting);

    QObject::connect(&m_keepAliveTimer, &QTimer::timeout, this,
                     &AllenHeathMidiProtocol::onKeepAliveTimeout);
}

AllenHeathMidiProtocol::~AllenHeathMidiProtocol() { disconnect(); }

bool AllenHeathMidiProtocol::connect(const QString& host, int port) {
    if (m_connectionState == ConnectionState::Connected ||
        m_connectionState == ConnectionState::Connecting) {
        disconnect();
    }

    m_host = host;
    m_port = port;

    setConnectionState(ConnectionState::Connecting);
    setStatus(QString("Connecting to %1:%2...").arg(host).arg(port));

    return m_transport.connect(host, port);
}

void AllenHeathMidiProtocol::disconnect() {
    m_keepAliveTimer.stop();
    m_transport.disconnect();

    m_parameterCache.clear();
    m_receiveBuffer.clear();
    m_latencyMs = 0;

    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathMidiProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    if (path.startsWith("/dca/")) {
        QStringList parts = path.split('/');
        if (parts.size() >= 4) {
            int dca = parts[2].toInt();
            QString param = parts[3];

            if (param == "fader") {
                // DCA fader: NRPN message
                // value is 0.0-1.0, convert to 14-bit MIDI value (0-16383)
                int midiValue = qBound(0, static_cast<int>(value.toFloat() * 16383.0f), 16383);
                int msb = (midiValue >> 7) & 0x7F;
                int lsb = midiValue & 0x7F;

                // Allen & Heath NRPN for DCA fader
                // NRPN MSB = 0x63, NRPN LSB varies by DCA
                QByteArray msg = buildNRPNMessage(0, 0x63, dca - 1, msb, lsb);
                m_transport.send(msg);
            } else if (param == "mute") {
                // DCA mute: Control Change
                int muteValue = value.toBool() ? 127 : 0;
                QByteArray msg = buildControlChange(0, 0x50 + dca - 1, muteValue);
                m_transport.send(msg);
            }
        }
    }

    m_parameterCache[path] = value;
}

QVariant AllenHeathMidiProtocol::getParameter(const QString& path) {
    return m_parameterCache.value(path);
}

void AllenHeathMidiProtocol::requestParameter(const QString& path) {
    // MIDI doesn't have a standard query mechanism
    // params are typically pushed by the console
    Q_UNUSED(path);
}

void AllenHeathMidiProtocol::requestParameterAsync(const QString& path,
                                                   ParameterCallback callback) {
    // return cached value if available
    if (m_parameterCache.contains(path)) {
        if (callback) {
            callback(path, m_parameterCache[path], true);
        }
    } else {
        if (callback) {
            callback(path, QVariant(), false);
        }
    }
}

void AllenHeathMidiProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    QJsonObject params = cue.parameters();
    for (auto it = params.begin(); it != params.end(); ++it) {
        sendParameter(it.key(), it.value().toVariant());
    }
}

void AllenHeathMidiProtocol::recallScene(int sceneNumber) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    // Allen & Heath SysEx scene recall
    // F0 00 00 1A 50 10 01 00 [scene] F7
    QByteArray msg = buildSysExSceneRecall(sceneNumber);
    m_transport.send(msg);
}

void AllenHeathMidiProtocol::refresh() {
    // MIDI doesn't support general refresh
    // console pushes updates automatically
}

QByteArray AllenHeathMidiProtocol::buildNRPNMessage(int channel, int nrpnMsb, int nrpnLsb,
                                                    int valueMsb, int valueLsb) {
    QByteArray msg;
    channel = channel & 0x0F;

    // NRPN MSB (CC 99)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(99));
    msg.append(static_cast<char>(nrpnMsb & 0x7F));

    // NRPN LSB (CC 98)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(98));
    msg.append(static_cast<char>(nrpnLsb & 0x7F));

    // data entry MSB (CC 6)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(6));
    msg.append(static_cast<char>(valueMsb & 0x7F));

    // data entry LSB (CC 38)
    msg.append(static_cast<char>(0xB0 | channel));
    msg.append(static_cast<char>(38));
    msg.append(static_cast<char>(valueLsb & 0x7F));

    return msg;
}

QByteArray AllenHeathMidiProtocol::buildSysExSceneRecall(int sceneNumber) {
    QByteArray msg;
    // Allen & Heath SysEx header
    msg.append(static_cast<char>(0xF0));               // SysEx start
    msg.append(static_cast<char>(0x00));               // manufacturer ID byte 1
    msg.append(static_cast<char>(0x00));               // manufacturer ID byte 2
    msg.append(static_cast<char>(0x1A));               // manufacturer ID byte 3 (Allen & Heath)
    msg.append(static_cast<char>(0x50));               // device ID (SQ/GLD family)
    msg.append(static_cast<char>(0x10));               // command: scene recall
    msg.append(static_cast<char>(0x01));               // sub-command
    msg.append(static_cast<char>(0x00));               // reserved
    msg.append(static_cast<char>(sceneNumber & 0x7F)); // scene number (0-indexed)
    msg.append(static_cast<char>(0xF7));               // SysEx end

    return msg;
}

QByteArray AllenHeathMidiProtocol::buildControlChange(int channel, int cc, int value) {
    QByteArray msg;
    msg.append(static_cast<char>(0xB0 | (channel & 0x0F)));
    msg.append(static_cast<char>(cc & 0x7F));
    msg.append(static_cast<char>(value & 0x7F));
    return msg;
}

void AllenHeathMidiProtocol::parseMidiData(const QByteArray& data) {
    m_receiveBuffer.append(data);

    while (!m_receiveBuffer.isEmpty()) {
        unsigned char status = static_cast<unsigned char>(m_receiveBuffer[0]);

        if (status == 0xF0) {
            int endPos = m_receiveBuffer.indexOf(static_cast<char>(0xF7));
            if (endPos < 0)
                break; // incomplete

            QByteArray sysex = m_receiveBuffer.left(endPos + 1);
            m_receiveBuffer.remove(0, endPos + 1);

            processSysEx(sysex);
        } else if ((status & 0xF0) == 0xB0) {
            if (m_receiveBuffer.size() < 3)
                break;

            int channel = status & 0x0F;
            int cc = static_cast<unsigned char>(m_receiveBuffer[1]);
            int value = static_cast<unsigned char>(m_receiveBuffer[2]);
            m_receiveBuffer.remove(0, 3);

            processControlChange(channel, cc, value);
        } else if ((status & 0x80) == 0) {
            m_receiveBuffer.remove(0, 1);
        } else {
            int len = 3;
            if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
                len = 2;
            }
            if (m_receiveBuffer.size() < len)
                break;
            m_receiveBuffer.remove(0, len);
        }
    }
}

void AllenHeathMidiProtocol::processControlChange(int channel, int cc, int value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_nrpnState.timestamp > 0 && (now - m_nrpnState.timestamp) > NRPN_TIMEOUT_MS) {
        m_nrpnState.reset();
    }

    // NRPN message sequence: CC 99 (MSB), CC 98 (LSB), CC 6 (data MSB), CC 38 (data LSB)
    switch (cc) {
    case 99:
        m_nrpnState.reset();
        m_nrpnState.channel = channel;
        m_nrpnState.nrpnMsb = value;
        m_nrpnState.timestamp = now;
        break;

    case 98:
        if (m_nrpnState.channel == channel) {
            m_nrpnState.nrpnLsb = value;
            m_nrpnState.timestamp = now;
        }
        break;

    case 6:
        if (m_nrpnState.channel == channel) {
            m_nrpnState.dataMsb = value;
            m_nrpnState.timestamp = now;
            if (m_nrpnState.isComplete()) {
                processNRPNComplete();
            }
        }
        break;

    case 38:
        if (m_nrpnState.channel == channel && m_nrpnState.dataMsb >= 0) {
            m_nrpnState.dataLsb = value;
            m_nrpnState.timestamp = now;
            if (m_nrpnState.isComplete()) {
                processNRPNComplete();
            }
        }
        break;

    default:
        // Allen & Heath DCA mutes: CC 0x50-0x57 (80-87) for DCAs 1-8
        if (cc >= 0x50 && cc <= 0x57) {
            int dca = cc - 0x50 + 1;
            bool muted = value >= 64;
            QString path = QString("/dca/%1/mute").arg(dca);
            m_parameterCache[path] = muted;
            emit parameterChanged(path, muted);
        }
        break;
    }
}

void AllenHeathMidiProtocol::processNRPNComplete() {
    int dataValue =
        (m_nrpnState.dataMsb << 7) | (m_nrpnState.dataLsb >= 0 ? m_nrpnState.dataLsb : 0);

    // Allen & Heath NRPN mapping for DCA faders
    // NRPN MSB 0x63 (99), LSB = DCA index (0-7 for DCAs 1-8)
    if (m_nrpnState.nrpnMsb == 0x63 && m_nrpnState.nrpnLsb >= 0 && m_nrpnState.nrpnLsb <= 7) {
        int dca = m_nrpnState.nrpnLsb + 1;
        float level = dataValue / 16383.0f;
        QString path = QString("/dca/%1/fader").arg(dca);
        m_parameterCache[path] = level;
        emit parameterChanged(path, level);
    }

    m_nrpnState.reset();
}

void AllenHeathMidiProtocol::processSysEx(const QByteArray& sysex) {
    // Allen & Heath SysEx format: F0 00 00 1A [device] [cmd] [data...] F7
    if (sysex.size() < 7)
        return;

    unsigned char byte3 = static_cast<unsigned char>(sysex[3]);
    if (byte3 != 0x1A)
        return; // not Allen & Heath

    unsigned char device = static_cast<unsigned char>(sysex[4]);
    unsigned char cmd = static_cast<unsigned char>(sysex[5]);

    Q_UNUSED(device);

    if (cmd == 0x10 && sysex.size() >= 9) {
        int sceneNumber = static_cast<unsigned char>(sysex[8]);
        emit sceneChanged(sceneNumber);
    }
}

void AllenHeathMidiProtocol::onTransportConnected() {
    setConnectionState(ConnectionState::Connected);
    setStatus(QString("Connected to %1:%2").arg(m_host).arg(m_port));

    m_keepAliveTimer.start(KEEPALIVE_INTERVAL);

    emit connected();
}

void AllenHeathMidiProtocol::onTransportDisconnected() {
    m_keepAliveTimer.stop();
    setConnectionState(ConnectionState::Disconnected);
    setStatus("Disconnected");
    emit disconnected();
}

void AllenHeathMidiProtocol::onTransportError(const QString& error) {
    setStatus(error);
    emit connectionError(error);
}

void AllenHeathMidiProtocol::onTransportConnectionLost() {
    setConnectionState(ConnectionState::Reconnecting);
    setStatus("Connection lost, reconnecting...");
    emit connectionLost();
}

void AllenHeathMidiProtocol::onDataReceived(const QByteArray& data) { parseMidiData(data); }

void AllenHeathMidiProtocol::onKeepAliveTimeout() {
    if (m_connectionState == ConnectionState::Connected) {
        // send active sensing (0xFE) as keep-alive
        QByteArray keepAlive;
        keepAlive.append(static_cast<char>(0xFE));
        m_transport.send(keepAlive);
    }
}

void AllenHeathMidiProtocol::onReconnecting(int attempt, int maxAttempts) {
    setStatus(QString("Reconnecting (attempt %1/%2)...").arg(attempt).arg(maxAttempts));
}

void AllenHeathMidiProtocol::setStatus(const QString& status) {
    if (m_statusMessage != status) {
        m_statusMessage = status;
        emit connectionStatusChanged(status);
    }
}

void AllenHeathMidiProtocol::setConnectionState(ConnectionState state) {
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

} // namespace OpenMix
