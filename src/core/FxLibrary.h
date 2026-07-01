#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

namespace OpenMix {

// A named FX unit (reverb/delay/etc.) identified by its console index.
struct FxUnit {
    int index = 0;
    QString name;

    QJsonObject toJson() const;
    [[nodiscard]] static FxUnit fromJson(const QJsonObject& json);
};

// Show-owned FX setup: the named FX units, which channels send to which units,
// and a default unit. Cues carry per-unit mute state separately (Cue::fxMutes).
class FxLibrary : public QObject {
    Q_OBJECT

  public:
    explicit FxLibrary(QObject* parent = nullptr);

    // named units (index -> name)
    void setUnit(int index, const QString& name); // add or rename
    void removeUnit(int index);
    [[nodiscard]] QString unitName(int index) const;
    [[nodiscard]] bool hasUnit(int index) const { return m_units.contains(index); }
    [[nodiscard]] QList<FxUnit> units() const; // sorted by index

    // channel -> FX units it sends to
    void setChannelAssignment(int channel, const QList<int>& fxUnits);
    [[nodiscard]] QList<int> channelAssignment(int channel) const;
    [[nodiscard]] QMap<int, QList<int>> assignments() const { return m_assignments; }
    void clearChannelAssignment(int channel);

    // default FX unit applied to newly assigned channels (-1 = none)
    [[nodiscard]] int defaultFx() const noexcept { return m_defaultFx; }
    void setDefaultFx(int index);

    [[nodiscard]] bool isEmpty() const;
    void clear();

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed();

  private:
    QMap<int, QString> m_units;            // FX unit index -> name
    QMap<int, QList<int>> m_assignments;   // channel -> [FX unit index, ...]
    int m_defaultFx = -1;
};

} // namespace OpenMix
