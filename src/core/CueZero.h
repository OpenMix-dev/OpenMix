#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>

namespace OpenMix {

class MixerProtocol;

// Cue Zero (TheatreMix): the show's base / reset state. Recalling it returns the
// console to a known starting point before the first real cue -- an optional
// base scene plus a set of "level" parameters and "label" (name) parameters.
// Its parameters double as PlaybackGuard safe-values (what PANIC restores to).
class CueZero : public QObject {
    Q_OBJECT

  public:
    explicit CueZero(QObject* parent = nullptr);

    // optional base scene to recall first (-1 = none)
    [[nodiscard]] int baseScene() const noexcept { return m_baseScene; }
    void setBaseScene(int scene);

    // level parameters (OSC-ish path -> numeric value, e.g. "/ch/01/mix/fader" -> 0.5)
    [[nodiscard]] QJsonObject levels() const { return m_levels; }
    void setLevels(const QJsonObject& levels);
    void setLevel(const QString& path, const QVariant& value);

    // label parameters (OSC-ish path -> string, e.g. "/ch/01/config/name" -> "Vocal")
    [[nodiscard]] QJsonObject labels() const { return m_labels; }
    void setLabels(const QJsonObject& labels);
    void setLabel(const QString& path, const QString& name);

    [[nodiscard]] bool isEmpty() const;
    void clear();

    // push base scene + levels + labels to a connected mixer
    void apply(MixerProtocol* mixer) const;

    // flattened levels + labels, suitable for PlaybackGuard::setDefaultSafeValues
    [[nodiscard]] QJsonObject safeValues() const;

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed();

  private:
    int m_baseScene = -1;
    QJsonObject m_levels;
    QJsonObject m_labels;
};

} // namespace OpenMix
