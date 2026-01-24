#include "LoopbackProtocol.h"
#include "../core/Cue.h"
#include <QJsonObject>
#include <QTimer>

namespace OpenMix {

LoopbackProtocol::LoopbackProtocol(const MixerCapabilities& caps, QObject* parent)
    : MixerProtocol(parent), m_capabilities(caps) {
    initializeDefaultState();
}

void LoopbackProtocol::initializeDefaultState() {
    // default EQ frequencies (6-band setup)
    static const double defaultFreqs[6] = {80.0, 220.0, 1000.0, 2500.0, 6000.0, 12000.0};
    // default EQ types: 0 = LCut, 1 = LShv, 2 = PEQ, 3 = VEQ, 4 = HShv, 5 = HCut
    static const int defaultTypes[6] = {1, 2, 2, 2, 2, 4};

    // init DCAs with default values
    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        QString prefix = QString("/dca/%1").arg(i);
        m_parameterState[prefix + "/fader"] = 0.75f; // 0dB
        m_parameterState[prefix + "/on"] = 1;        // unmuted
        m_parameterState[prefix + "/config/name"] = QString("DCA %1").arg(i);
    }

    // init input channels for testing
    for (int i = 1; i <= qMin(m_capabilities.inputChannels, 32); ++i) {
        QString chPrefix = QString("/ch/%1").arg(i, 2, 10, QChar('0'));
        m_parameterState[chPrefix + "/mix/fader"] = 0.75f;
        m_parameterState[chPrefix + "/mix/on"] = 1;
        m_parameterState[chPrefix + "/config/name"] = QString("Ch %1").arg(i);

        // EQ parameters
        if (m_capabilities.supportsChannelEQ) {
            m_parameterState[chPrefix + "/eq/on"] = 1; // EQ enabled

            for (int band = 1; band <= m_capabilities.eqBandsPerChannel && band <= 6; ++band) {
                QString bandPrefix = QString("%1/eq/%2").arg(chPrefix).arg(band);
                m_parameterState[bandPrefix + "/type"] = defaultTypes[band - 1];
                m_parameterState[bandPrefix + "/f"] = defaultFreqs[band - 1];
                m_parameterState[bandPrefix + "/g"] = 0.0f; // 0dB (unity)
                m_parameterState[bandPrefix + "/q"] = 2.0f; // moderate Q
            }
        }

        // effect send parameters
        if (m_capabilities.supportsEffectSends) {
            int sends = qMin(m_capabilities.effectSendBuses, 16);
            for (int send = 1; send <= sends; ++send) {
                QString sendPrefix =
                    QString("%1/mix/%2").arg(chPrefix).arg(send, 2, 10, QChar('0'));
                m_parameterState[sendPrefix + "/level"] = 0.0f; // -inf dB
                m_parameterState[sendPrefix + "/on"] = 0;       // disabled
            }
        }
    }

    // init mix buses with EQ
    for (int i = 1; i <= qMin(m_capabilities.mixBuses, 16); ++i) {
        QString busPrefix = QString("/bus/%1").arg(i, 2, 10, QChar('0'));
        m_parameterState[busPrefix + "/mix/fader"] = 0.75f;
        m_parameterState[busPrefix + "/mix/on"] = 1;
        m_parameterState[busPrefix + "/config/name"] = QString("Bus %1").arg(i);

        // bus EQ parameters
        if (m_capabilities.supportsChannelEQ) {
            m_parameterState[busPrefix + "/eq/on"] = 1;

            for (int band = 1; band <= m_capabilities.eqBandsPerChannel && band <= 6; ++band) {
                QString bandPrefix = QString("%1/eq/%2").arg(busPrefix).arg(band);
                m_parameterState[bandPrefix + "/type"] = defaultTypes[band - 1];
                m_parameterState[bandPrefix + "/f"] = defaultFreqs[band - 1];
                m_parameterState[bandPrefix + "/g"] = 0.0f;
                m_parameterState[bandPrefix + "/q"] = 2.0f;
            }
        }
    }

    // init main stereo bus
    m_parameterState["/main/st/mix/fader"] = 0.75f;
    m_parameterState["/main/st/mix/on"] = 1;

    if (m_capabilities.supportsChannelEQ) {
        m_parameterState["/main/st/eq/on"] = 1;

        for (int band = 1; band <= m_capabilities.eqBandsPerChannel && band <= 6; ++band) {
            QString bandPrefix = QString("/main/st/eq/%1").arg(band);
            m_parameterState[bandPrefix + "/type"] = defaultTypes[band - 1];
            m_parameterState[bandPrefix + "/f"] = defaultFreqs[band - 1];
            m_parameterState[bandPrefix + "/g"] = 0.0f;
            m_parameterState[bandPrefix + "/q"] = 2.0f;
        }
    }
}

bool LoopbackProtocol::connect(const QString& host, int port) {
    Q_UNUSED(host);
    Q_UNUSED(port);

    m_connected = true;
    m_statusMessage = "Connected (Offline)";

    // emit connection signal on next event loop
    QTimer::singleShot(0, this, [this]() {
        emit connectionStateChanged(ConnectionState::Connected);
        emit connectionStatusChanged(m_statusMessage);
        emit connected();

        // emit initial parameter values
        for (auto it = m_parameterState.begin(); it != m_parameterState.end(); ++it) {
            emit parameterChanged(it.key(), it.value());
        }
    });

    return true;
}

void LoopbackProtocol::disconnect() {
    m_connected = false;
    m_statusMessage = "Disconnected";

    emit connectionStateChanged(ConnectionState::Disconnected);
    emit connectionStatusChanged(m_statusMessage);
    emit disconnected();
}

void LoopbackProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (!m_connected) {
        return;
    }

    m_parameterState[path] = value;

    // echo back the change
    QTimer::singleShot(5, this, [this, path, value]() { emit parameterChanged(path, value); });
}

QVariant LoopbackProtocol::getParameter(const QString& path) {
    return m_parameterState.value(path);
}

void LoopbackProtocol::requestParameter(const QString& path) {
    if (!m_connected) {
        return;
    }

    QVariant value = m_parameterState.value(path);

    // respond asynchronously
    QTimer::singleShot(5, this, [this, path, value]() { emit parameterChanged(path, value); });
}

void LoopbackProtocol::requestParameterAsync(const QString& path, ParameterCallback callback) {
    if (!m_connected) {
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

void LoopbackProtocol::recallSnapshot(const Cue& cue) {
    QJsonObject params = cue.parameters();

    // recall DCA parameters
    if (params.contains("dca")) {
        QJsonObject dcaObj = params["dca"].toObject();
        for (auto it = dcaObj.begin(); it != dcaObj.end(); ++it) {
            int dcaNum = it.key().toInt();
            QJsonObject dcaParams = it.value().toObject();
            QString prefix = QString("/dca/%1").arg(dcaNum);

            for (auto pit = dcaParams.begin(); pit != dcaParams.end(); ++pit) {
                QString path = prefix + "/" + pit.key();
                QVariant value = pit.value().toVariant();
                sendParameter(path, value);
            }
        }
    }
}

void LoopbackProtocol::recallScene(int sceneNumber) {
    Q_UNUSED(sceneNumber);
    emit sceneChanged(sceneNumber);
}

void LoopbackProtocol::refresh() {
    // do nothing
}

} // namespace OpenMix
