#include "PlaybackGuard.h"
#include "PlaybackEngine.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <QTimer>

namespace OpenMix {

PlaybackGuard::PlaybackGuard(PlaybackEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    if (m_engine) {
        connect(m_engine, &PlaybackEngine::fadeProgressChanged, this,
                &PlaybackGuard::onFadeProgressChanged);
        connect(m_engine, &PlaybackEngine::panicFadeCompleted, this,
                &PlaybackGuard::onPanicFadeComplete);
    }

    m_defaultSafeValues = QJsonObject();
}

void PlaybackGuard::setMixer(MixerProtocol* mixer) { m_mixer = mixer; }

void PlaybackGuard::setLockoutEnabled(bool enabled) {
    m_lockoutEnabled = enabled;
    if (!enabled && m_wasLocked) {
        m_wasLocked = false;
        emit lockoutStateChanged(false);
    }
}

void PlaybackGuard::setLockoutThreshold(double progressPercent) {
    m_lockoutThreshold = qBound(0.0, progressPercent, 1.0);
}

bool PlaybackGuard::isGoLocked() const {
    if (!m_lockoutEnabled || !m_engine) {
        return false;
    }

    if (m_engine->activeFadeCount() > 0 && m_engine->fadeProgress() < m_lockoutThreshold) {
        return true;
    }

    if (m_panicActive) {
        return true;
    }

    return false;
}

QString PlaybackGuard::lockoutReason() const {
    if (m_panicActive) {
        return tr("PANIC mode active - please wait");
    }

    if (m_lockoutEnabled && m_engine && m_engine->activeFadeCount() > 0) {
        if (m_engine->fadeProgress() < m_lockoutThreshold) {
            return tr("Fade in progress (%1%) - below safety threshold (%2%)")
                .arg(static_cast<int>(m_engine->fadeProgress() * 100))
                .arg(static_cast<int>(m_lockoutThreshold * 100));
        }
    }

    return QString();
}

void PlaybackGuard::captureSafeState(const QString& description) {
    if (!m_mixer)
        return;

    m_safeState.parameters = m_mixer->captureCurrentState();
    m_safeState.timestamp = QDateTime::currentMSecsSinceEpoch();
    m_safeState.sourceDescription = description;

    emit safeStateCaptured(description);
}

void PlaybackGuard::clearSafeState() { m_safeState = SafeState(); }

void PlaybackGuard::setDefaultSafeValues(const QJsonObject& values) {
    m_defaultSafeValues = values;
}

void PlaybackGuard::panic(double fadeOutSeconds) {
    if (m_panicActive)
        return;

    if (m_mixer) {
        m_prePanicState.parameters = m_mixer->captureCurrentState();
        m_prePanicState.timestamp = QDateTime::currentMSecsSinceEpoch();
        m_prePanicState.sourceDescription = tr("Pre-panic state");
    }

    m_panicActive = true;
    m_restorationPending = false;

    if (m_engine) {
        m_engine->stop();
    }

    emit panicTriggered();

    if (fadeOutSeconds > 0) {
        executePanicFade(fadeOutSeconds);
    } else {
        panicImmediate();
    }
}

void PlaybackGuard::panicImmediate() {
    if (!m_panicActive) {
        if (m_mixer) {
            m_prePanicState.parameters = m_mixer->captureCurrentState();
            m_prePanicState.timestamp = QDateTime::currentMSecsSinceEpoch();
            m_prePanicState.sourceDescription = tr("Pre-panic state");
        }

        m_panicActive = true;

        if (m_engine) {
            m_engine->stop();
        }

        emit panicTriggered();
    }

    sendSafeValues();

    emit panicCompleted();
}

void PlaybackGuard::panicAndRestore(double fadeOutSeconds) {
    m_restorationPending = true;
    panic(fadeOutSeconds);
}

void PlaybackGuard::restoreFromPanic(double fadeInSeconds) {
    if (!m_panicActive)
        return;

    if (m_prePanicState.parameters.isEmpty()) {
        m_panicActive = false;
        return;
    }

    emit restorationStarted();

    if (fadeInSeconds > 0) {
        executeRestoreFade(fadeInSeconds);
    } else {
        if (m_mixer) {
            for (auto it = m_prePanicState.parameters.begin();
                 it != m_prePanicState.parameters.end(); ++it) {
                m_mixer->sendParameter(it.key(), it.value().toVariant());
            }
        }
        m_panicActive = false;
        emit restorationCompleted();
    }
}

void PlaybackGuard::onFadeProgressChanged(double progress) {
    if (!m_lockoutEnabled)
        return;

    bool nowLocked = isGoLocked();
    if (nowLocked != m_wasLocked) {
        m_wasLocked = nowLocked;
        emit lockoutStateChanged(nowLocked);
    }
}

void PlaybackGuard::onPanicFadeComplete() {
    if (m_restoreFadeActive) {
        m_restoreFadeActive = false;
        onRestoreFadeComplete();
        return;
    }

    emit panicCompleted();

    if (m_restorationPending) {
        m_restorationPending = false;
        QTimer::singleShot(500, this, [this]() { restoreFromPanic(1.0); });
    }
}

void PlaybackGuard::onRestoreFadeComplete() {
    m_panicActive = false;
    m_restorationPending = false;
    emit restorationCompleted();
}

void PlaybackGuard::executePanicFade(double seconds) {
    if (!m_engine) {
        panicImmediate();
        return;
    }

    QJsonObject targetValues = m_defaultSafeValues;

    if (hasSafeState()) {
        targetValues = m_safeState.parameters;
    }

    if (targetValues.isEmpty()) {
        panicImmediate();
        return;
    }

    m_engine->executePanicFade(targetValues, seconds, m_panicFadeCurve);
}

void PlaybackGuard::executeRestoreFade(double seconds) {
    if (!m_engine || m_prePanicState.parameters.isEmpty()) {
        onRestoreFadeComplete();
        return;
    }

    m_restoreFadeActive = true;
    m_engine->executePanicFade(m_prePanicState.parameters, seconds, m_panicFadeCurve);
}

void PlaybackGuard::sendSafeValues() {
    if (!m_mixer)
        return;

    QJsonObject values = hasSafeState() ? m_safeState.parameters : m_defaultSafeValues;

    for (auto it = values.begin(); it != values.end(); ++it) {
        m_mixer->sendParameter(it.key(), it.value().toVariant());
    }
}

} // namespace OpenMix
