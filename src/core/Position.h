#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QUuid>
#include <optional>

namespace OpenMix {

// A named spatial position: a reusable pan/delay
// preset that a cue can assign to one or more input channels so an actor's
// voice images to a consistent place in the room.
class Position {
  public:
    Position();
    explicit Position(const QString& name, const QString& shortName = QString());

    [[nodiscard]] QString id() const { return m_id; }
    void regenerateId();

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    [[nodiscard]] QString shortName() const { return m_shortName; }
    void setShortName(const QString& shortName) { m_shortName = shortName; }

    // delay in milliseconds (image-shifting / time alignment)
    [[nodiscard]] double delay() const noexcept { return m_delay; }
    void setDelay(double ms) { m_delay = ms; }

    // pan, normalized -1.0 (full left) .. 0.0 (centre) .. +1.0 (full right)
    [[nodiscard]] double pan() const noexcept { return m_pan; }
    void setPan(double pan) { m_pan = pan; }

    // buses this position images to (e.g. surround/LCR sends); empty = main only
    [[nodiscard]] QList<int> buses() const { return m_buses; }
    void setBuses(const QList<int>& buses) { m_buses = buses; }

    QJsonObject toJson() const;
    [[nodiscard]] static Position fromJson(const QJsonObject& json);

    bool operator==(const Position& other) const noexcept { return m_id == other.m_id; }

  private:
    QString m_id;
    QString m_name;
    QString m_shortName;
    double m_delay = 0.0;
    double m_pan = 0.0;
    QList<int> m_buses;
};

// Show-owned library of named positions. Mirrors DCAMapping: holds the data,
// emits change signals, and serializes via toJson/loadFromJson.
class PositionLibrary : public QObject {
    Q_OBJECT

  public:
    explicit PositionLibrary(QObject* parent = nullptr);

    // mutation (each emits + returns the position id)
    QString addPosition(const Position& position);
    void updatePosition(const Position& position);
    bool removePosition(const QString& id);
    void setPositions(const QList<Position>& positions);
    void clear();

    // query
    [[nodiscard]] std::optional<Position> position(const QString& id) const;
    [[nodiscard]] QList<Position> positions() const { return m_positions; }
    [[nodiscard]] int count() const { return m_positions.size(); }
    [[nodiscard]] bool isEmpty() const { return m_positions.isEmpty(); }
    [[nodiscard]] bool contains(const QString& id) const;

    // serialization
    QJsonObject toJson() const;
    [[nodiscard]] static PositionLibrary* fromJson(const QJsonObject& json, QObject* parent = nullptr);
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed(); // any mutation (Show connects this to its dirty flag)
    void positionAdded(const QString& id);
    void positionRemoved(const QString& id);
    void positionChanged(const QString& id);
    void libraryCleared();

  private:
    [[nodiscard]] int indexOf(const QString& id) const;

    QList<Position> m_positions;
};

} // namespace OpenMix
