#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

namespace OpenMix {

class MixerProtocol;

class PreviewLayer : public QObject {
    Q_OBJECT

  public:
    explicit PreviewLayer(QObject* parent = nullptr);

    void setMixer(MixerProtocol* mixer);

    // enable/disable preview mode
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // parameter interception
    void sendParameter(const QString& path, const QVariant& value);
    QVariant cachedValue(const QString& path) const;
    bool hasCachedValue(const QString& path) const;

    // cached value management
    QStringList cachedPaths() const;
    int cachedCount() const;

    // flush cached values to mixer
    void flush();

    // discard cached values w/o sending
    void discard();

  signals:
    void enabledChanged(bool enabled);
    void parameterCached(const QString& path, const QVariant& value);
    void cacheCleared();
    void cacheFlushed();

  private:
    bool m_enabled = false;
    MixerProtocol* m_mixer = nullptr;
    QMap<QString, QVariant> m_cachedValues;
};

} // namespace OpenMix
