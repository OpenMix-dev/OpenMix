#include "PlaybackGuard.h"
#include "PlaybackEngine.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>
#include <QTimer>

namespace OpenMix {

PlaybackGuard::PlaybackGuard(PlaybackEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
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

bool PlaybackGuard::isGoLocked() const {
    if (!m_lockoutEnabled || !m_engine) {
        return false;
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

    return QString();
}

void PlaybackGuard::setDefaultSafeValues(const QJsonObject& values) {
    m_defaultSafeValues = values;
}

void PlaybackGuard::panic() {
    if (m_panicActive)
        return;

    m_panicActive = true;

    if (m_engine) {
        m_engine->stop();
    }

    emit panicTriggered();

    sendSafeValues();

    m_panicActive = false;

    emit panicCompleted();
}

void PlaybackGuard::sendSafeValues() {
    if (!m_mixer)
        return;

    for (const auto& [path, value] : m_defaultSafeValues.asKeyValueRange()) {
        m_mixer->sendParameter(path.toString(), value.toVariant());
    }
}

} // namespace OpenMix
