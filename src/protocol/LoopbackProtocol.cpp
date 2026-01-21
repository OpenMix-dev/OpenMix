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
    // inite DCAs with default values
    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        QString prefix = QString("/dca/%1").arg(i);
        m_parameterState[prefix + "/fader"] = 0.75f; // 0dB
        m_parameterState[prefix + "/on"] = 1;        // unmuted
        m_parameterState[prefix + "/config/name"] = QString("DCA %1").arg(i);
    }

    // init some input channels for testing
    for (int i = 1; i <= qMin(m_capabilities.inputChannels, 32); ++i) {
        QString prefix = QString("/ch/%1").arg(i, 2, 10, QChar('0'));
        m_parameterState[prefix + "/mix/fader"] = 0.75f;
        m_parameterState[prefix + "/mix/on"] = 1;
        m_parameterState[prefix + "/config/name"] = QString("Ch %1").arg(i);
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

void LoopbackProtocol::captureSnapshot(Cue& cue) {
    QJsonObject params;

    // capture DCA state
    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        QString prefix = QString("/dca/%1").arg(i);
        QJsonObject dcaParams;

        if (m_parameterState.contains(prefix + "/fader")) {
            dcaParams["fader"] = m_parameterState[prefix + "/fader"].toDouble();
        }
        if (m_parameterState.contains(prefix + "/on")) {
            dcaParams["on"] = m_parameterState[prefix + "/on"].toInt();
        }

        if (!dcaParams.isEmpty()) {
            QJsonObject dcaContainer = params["dca"].toObject();
            dcaContainer[QString::number(i)] = dcaParams;
            params["dca"] = dcaContainer;
        }
    }

    cue.setParameters(params);
    emit snapshotCaptured();
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

QJsonObject LoopbackProtocol::captureCurrentState() {
    QJsonObject state;

    for (auto it = m_parameterState.begin(); it != m_parameterState.end(); ++it) {
        state[it.key()] = QJsonValue::fromVariant(it.value());
    }

    return state;
}

void LoopbackProtocol::recallScene(int sceneNumber) {
    Q_UNUSED(sceneNumber);
    emit sceneChanged(sceneNumber);
}

void LoopbackProtocol::refresh() {
    // do nothing
}

} // namespace OpenMix
