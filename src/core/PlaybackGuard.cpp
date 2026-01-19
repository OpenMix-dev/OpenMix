#include "PlaybackGuard.h"
#include "PlaybackEngine.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <QTimer>

namespace StageBlend {

PlaybackGuard::PlaybackGuard(PlaybackEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    if (m_engine) {
        connect(m_engine, &PlaybackEngine::fadeProgressChanged, this,
                &PlaybackGuard::onFadeProgressChanged);
    }

    // initialize default safe values (faders to 0, etc.)
    // these can be overridden by the application
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

    // lock if there are active fades below the threshold
    if (m_engine->activeFadeCount() > 0 && m_engine->fadeProgress() < m_lockoutThreshold) {
        return true;
    }

    // lock during panic
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

    // capture current state before panic for potential restoration
    if (m_mixer) {
        m_prePanicState.parameters = m_mixer->captureCurrentState();
        m_prePanicState.timestamp = QDateTime::currentMSecsSinceEpoch();
        m_prePanicState.sourceDescription = tr("Pre-panic state");
    }

    m_panicActive = true;
    m_restorationPending = false;

    // stop all current playback
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
        // capture current state before panic
        if (m_mixer) {
            m_prePanicState.parameters = m_mixer->captureCurrentState();
            m_prePanicState.timestamp = QDateTime::currentMSecsSinceEpoch();
            m_prePanicState.sourceDescription = tr("Pre-panic state");
        }

        m_panicActive = true;

        // stop all current playback
        if (m_engine) {
            m_engine->stop();
        }

        emit panicTriggered();
    }

    // immediately send safe values
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
        // no pre-panic state to restore, just clear panic
        m_panicActive = false;
        return;
    }

    emit restorationStarted();

    if (fadeInSeconds > 0) {
        executeRestoreFade(fadeInSeconds);
    } else {
        // immediate restore
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
    emit panicCompleted();

    if (m_restorationPending) {
        // auto-restore after panic completes
        QTimer::singleShot(500, this, [this]() { restoreFromPanic(1.0); });
    }
}

void PlaybackGuard::onRestoreFadeComplete() {
    m_panicActive = false;
    m_restorationPending = false;
    emit restorationCompleted();
}

void PlaybackGuard::executePanicFade(double seconds) {
    if (!m_mixer) {
        panicImmediate();
        return;
    }

    // determine target values (use default safe values or captured safe state)
    QJsonObject targetValues = m_defaultSafeValues;

    // if we have a captured safe state, prefer that
    if (hasSafeState()) {
        targetValues = m_safeState.parameters;
    }

    if (targetValues.isEmpty()) {
        // no safe values defined, just stop
        panicImmediate();
        return;
    }

    // simple linear fade implementation using a timer
    // in a more complete implementation, this would integrate with PlaybackEngine
    const int steps = qMax(1, static_cast<int>(seconds * 60)); // 60fps
    const int interval = static_cast<int>(seconds * 1000 / steps);

    QJsonObject startValues = m_mixer->captureCurrentState();

    // create a timer for the fade
    int* currentStep = new int(0);
    QTimer* fadeTimer = new QTimer(this);
    fadeTimer->setInterval(interval);

    connect(fadeTimer, &QTimer::timeout, this, [=]() mutable {
        (*currentStep)++;
        double progress = static_cast<double>(*currentStep) / steps;

        if (progress >= 1.0) {
            // final step - send exact target values
            for (auto it = targetValues.begin(); it != targetValues.end(); ++it) {
                m_mixer->sendParameter(it.key(), it.value().toVariant());
            }
            fadeTimer->stop();
            fadeTimer->deleteLater();
            delete currentStep;
            onPanicFadeComplete();
            return;
        }

        // interpolate values
        for (auto it = targetValues.begin(); it != targetValues.end(); ++it) {
            QString path = it.key();
            QVariant endVal = it.value().toVariant();

            if (endVal.typeId() == QMetaType::Double || endVal.typeId() == QMetaType::Float) {
                double startVal = startValues.value(path).toDouble();
                double targetVal = endVal.toDouble();
                double currentVal = startVal + (targetVal - startVal) * progress;
                m_mixer->sendParameter(path, currentVal);
            } else if (endVal.typeId() == QMetaType::Int) {
                // for integers, switch at midpoint
                if (progress >= 0.5) {
                    m_mixer->sendParameter(path, endVal.toInt());
                }
            }
        }
    });

    fadeTimer->start();
}

void PlaybackGuard::executeRestoreFade(double seconds) {
    if (!m_mixer || m_prePanicState.parameters.isEmpty()) {
        onRestoreFadeComplete();
        return;
    }

    const int steps = qMax(1, static_cast<int>(seconds * 60));
    const int interval = static_cast<int>(seconds * 1000 / steps);

    QJsonObject startValues = m_mixer->captureCurrentState();
    QJsonObject targetValues = m_prePanicState.parameters;

    int* currentStep = new int(0);
    QTimer* fadeTimer = new QTimer(this);
    fadeTimer->setInterval(interval);

    connect(fadeTimer, &QTimer::timeout, this, [=]() mutable {
        (*currentStep)++;
        double progress = static_cast<double>(*currentStep) / steps;

        if (progress >= 1.0) {
            for (auto it = targetValues.begin(); it != targetValues.end(); ++it) {
                m_mixer->sendParameter(it.key(), it.value().toVariant());
            }
            fadeTimer->stop();
            fadeTimer->deleteLater();
            delete currentStep;
            onRestoreFadeComplete();
            return;
        }

        for (auto it = targetValues.begin(); it != targetValues.end(); ++it) {
            QString path = it.key();
            QVariant endVal = it.value().toVariant();

            if (endVal.typeId() == QMetaType::Double || endVal.typeId() == QMetaType::Float) {
                double startVal = startValues.value(path).toDouble();
                double targetVal = endVal.toDouble();
                double currentVal = startVal + (targetVal - startVal) * progress;
                m_mixer->sendParameter(path, currentVal);
            } else if (endVal.typeId() == QMetaType::Int) {
                if (progress >= 0.5) {
                    m_mixer->sendParameter(path, endVal.toInt());
                }
            }
        }
    });

    fadeTimer->start();
}

void PlaybackGuard::sendSafeValues() {
    if (!m_mixer)
        return;

    // first try captured safe state, then default safe values
    QJsonObject values = hasSafeState() ? m_safeState.parameters : m_defaultSafeValues;

    for (auto it = values.begin(); it != values.end(); ++it) {
        m_mixer->sendParameter(it.key(), it.value().toVariant());
    }
}

} // namespace StageBlend
