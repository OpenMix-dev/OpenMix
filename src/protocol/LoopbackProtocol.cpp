#include "LoopbackProtocol.h"
#include "../core/Cue.h"
#include <QJsonObject>
#include <QTimer>
#include <algorithm>
#include <array>

namespace OpenMix {

LoopbackProtocol::LoopbackProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps) {
    initializeDefaultState();
}

void LoopbackProtocol::initializeDefaultState() {
    // 0=LCut 1=LShv 2=PEQ 3=VEQ 4=HShv 5=HCut
    static constexpr std::array<double, 6> defaultFreqs = {80.0, 220.0, 1000.0, 2500.0, 6000.0, 12000.0};
    static constexpr std::array<int, 6>    defaultTypes = {1, 2, 2, 2, 2, 4};

    auto initEQ = [&](const QString& prefix) {
        if (!m_capabilities.supportsChannelEQ)
            return;
        m_parameterState[prefix + "/eq/on"] = 1;
        for (int band = 1; band <= m_capabilities.eqBandsPerChannel && band <= 6; ++band) {
            const QString bandPrefix = QStringLiteral("%1/eq/%2").arg(prefix).arg(band);
            m_parameterState[bandPrefix + "/type"] = defaultTypes[band - 1];
            m_parameterState[bandPrefix + "/f"]    = defaultFreqs[band - 1];
            m_parameterState[bandPrefix + "/g"]    = 0.0f;
            m_parameterState[bandPrefix + "/q"]    = 2.0f;
        }
    };

    // init DCAs with default values
    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        QString prefix = QString("/dca/%1").arg(i);
        m_parameterState[prefix + "/fader"] = 0.75f; // 0dB
        m_parameterState[prefix + "/on"] = 1;        // unmuted
        m_parameterState[prefix + "/config/name"] = QString("DCA %1").arg(i);
    }

    // init input channels for testing
    for (int i = 1; i <= std::min(m_capabilities.inputChannels, 32); ++i) {
        QString chPrefix = QString("/ch/%1").arg(i, 2, 10, QChar('0'));
        m_parameterState[chPrefix + "/mix/fader"] = 0.75f;
        m_parameterState[chPrefix + "/mix/on"] = 1;
        m_parameterState[chPrefix + "/config/name"] = QString("Ch %1").arg(i);

        // DCA group assignment bitmask (assign ch 1-4 to DCA1, 5-8 to DCA2, etc.)
        int dcaIndex = ((i - 1) / 4) % m_capabilities.dcaCount;
        m_parameterState[chPrefix + "/grp/dca"] = (1 << dcaIndex);

        initEQ(chPrefix);

        // effect send parameters
        if (m_capabilities.supportsEffectSends) {
            int sends = std::min(m_capabilities.effectSendBuses, 16);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_parameterState[sendPrefix + "/level"] = 0.0f; // -inf dB
                m_parameterState[sendPrefix + "/on"] = 0;       // disabled
            }
        }
    }

    // init mix buses with EQ
    for (int i = 1; i <= std::min(m_capabilities.mixBuses, 16); ++i) {
        QString busPrefix = QString("/bus/%1").arg(i, 2, 10, QChar('0'));
        m_parameterState[busPrefix + "/mix/fader"] = 0.75f;
        m_parameterState[busPrefix + "/mix/on"] = 1;
        m_parameterState[busPrefix + "/config/name"] = QString("Bus %1").arg(i);

        // DCA group assignment bitmask (assign bus 1-2 to DCA1, 3-4 to DCA2, etc.)
        int dcaIndex = ((i - 1) / 2) % m_capabilities.dcaCount;
        m_parameterState[busPrefix + "/grp/dca"] = (1 << dcaIndex);

        initEQ(busPrefix);
    }

    // init main stereo bus
    m_parameterState["/main/st/mix/fader"] = 0.75f;
    m_parameterState["/main/st/mix/on"] = 1;

    initEQ("/main/st");
}

bool LoopbackProtocol::connect([[maybe_unused]] const QString& host, [[maybe_unused]] int port) {
    m_connectionState = ConnectionState::Connected;
    m_statusMessage = "Connected (Offline)";

    // emit connection signal on next event loop
    QTimer::singleShot(0, this, [this]() {
        emit connectionStateChanged(ConnectionState::Connected);
        emit connectionStatusChanged(m_statusMessage);
        emit connected();

        // emit initial parameter values
        for (const auto& [path, value] : m_parameterState.asKeyValueRange()) {
            emit parameterChanged(path, value);
        }
    });

    return true;
}

void LoopbackProtocol::disconnect() {
    m_connectionState = ConnectionState::Disconnected;
    m_statusMessage = "Disconnected";

    emit connectionStateChanged(ConnectionState::Disconnected);
    emit connectionStatusChanged(m_statusMessage);
    emit disconnected();
}

void LoopbackProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    m_parameterState[path] = value;

    // echo back the change
    emitParameterChangedAsync(path, value);
}

QVariant LoopbackProtocol::getParameter(const QString& path) {
    return m_parameterState.value(path);
}

void LoopbackProtocol::requestParameter(const QString& path) {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    QVariant value = m_parameterState.value(path);

    // respond asynchronously
    emitParameterChangedAsync(path, value);
}

void LoopbackProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (m_connectionState != ConnectionState::Connected) {
        if (callback) {
            callback(path, QVariant(), false);
        }
        return;
    }

    QVariant value = m_parameterState.value(path);

    QTimer::singleShot(5, this, [this, path, value, callback]() {
        emit parameterChanged(path, value);
        if (callback) {
            callback(path, value, true);
        }
    });
}

void LoopbackProtocol::emitParameterChangedAsync(const QString& addr, const QVariant& val) {
    QTimer::singleShot(5, this, [this, addr, val]() { emit parameterChanged(addr, val); });
}

void LoopbackProtocol::recallSnapshot(const Cue& cue) {
    if (m_connectionState != ConnectionState::Connected)
        return;

    for (const auto& [path, value] : cue.parameters().asKeyValueRange()) {
        sendParameter(path.toString(), value.toVariant());
    }
}

void LoopbackProtocol::recallScene(int sceneNumber) {
    emit sceneChanged(sceneNumber);
}

void LoopbackProtocol::refresh() {
    // do nothing
}

void LoopbackProtocol::setChannelFader(int channel, double level) {
    m_recordedCalls.append(QString("fader:ch=%1:level=%2").arg(channel).arg(level));
}

void LoopbackProtocol::setChannelMute(int channel, bool muted) {
    m_recordedCalls.append(QString("mute:ch=%1:muted=%2").arg(channel).arg(muted ? 1 : 0));
}

void LoopbackProtocol::setChannelPreamp(int channel, double gainDb) {
    m_recordedCalls.append(QString("preamp:ch=%1:gain=%2").arg(channel).arg(gainDb));
}

void LoopbackProtocol::setChannelHpf(int channel, bool on, double freqHz) {
    m_recordedCalls.append(
        QString("hpf:ch=%1:on=%2:freq=%3").arg(channel).arg(on ? 1 : 0).arg(freqHz));
}

void LoopbackProtocol::setChannelEqOn(int channel, bool on) {
    m_recordedCalls.append(QString("eqon:ch=%1:on=%2").arg(channel).arg(on ? 1 : 0));
}

void LoopbackProtocol::setChannelEqBand(int channel, int band, bool on, int type, double freqHz,
                                        double gainDb, double q) {
    m_recordedCalls.append(QString("eqband:ch=%1:band=%2:on=%3:type=%4:f=%5:g=%6:q=%7")
                               .arg(channel)
                               .arg(band)
                               .arg(on ? 1 : 0)
                               .arg(type)
                               .arg(freqHz)
                               .arg(gainDb)
                               .arg(q));
}

void LoopbackProtocol::setChannelDynamics(int channel, bool on, double thresholdDb, double ratio,
                                          double attackMs, double releaseMs, double makeupDb) {
    m_recordedCalls.append(QString("dyn:ch=%1:on=%2:thr=%3:ratio=%4:atk=%5:rel=%6:gain=%7")
                               .arg(channel)
                               .arg(on ? 1 : 0)
                               .arg(thresholdDb)
                               .arg(ratio)
                               .arg(attackMs)
                               .arg(releaseMs)
                               .arg(makeupDb));
}

} // namespace OpenMix
