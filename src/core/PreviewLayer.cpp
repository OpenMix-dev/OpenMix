#include "PreviewLayer.h"
#include "protocol/MixerProtocol.h"

namespace OpenMix {

PreviewLayer::PreviewLayer(QObject* parent) : QObject(parent) {}

void PreviewLayer::setMixer(MixerProtocol* mixer) { m_mixer = mixer; }

void PreviewLayer::setEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(enabled);
    }
}

void PreviewLayer::sendParameter(const QString& path, const QVariant& value) {
    if (m_enabled) {
        // cache value instead of sending to mixer
        m_cachedValues[path] = value;
        emit parameterCached(path, value);
    } else if (m_mixer) {
        // not in preview mode, send directly to mixer
        m_mixer->sendParameter(path, value);
    }
}

QVariant PreviewLayer::cachedValue(const QString& path) const { return m_cachedValues.value(path); }

bool PreviewLayer::hasCachedValue(const QString& path) const {
    return m_cachedValues.contains(path);
}

QStringList PreviewLayer::cachedPaths() const { return m_cachedValues.keys(); }

int PreviewLayer::cachedCount() const { return m_cachedValues.size(); }

void PreviewLayer::flush() {
    // send all cached values to mixer if available
    if (m_mixer && !m_cachedValues.isEmpty()) {
        for (auto it = m_cachedValues.begin(); it != m_cachedValues.end(); ++it) {
            m_mixer->sendParameter(it.key(), it.value());
        }
    }

    m_cachedValues.clear();
    emit cacheFlushed();
}

void PreviewLayer::discard() {
    m_cachedValues.clear();
    emit cacheCleared();
}

} // namespace OpenMix
