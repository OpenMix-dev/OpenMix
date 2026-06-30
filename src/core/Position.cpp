#include "Position.h"

namespace OpenMix {

Position::Position() : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

Position::Position(const QString& name, const QString& shortName)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_name(name),
      m_shortName(shortName) {}

void Position::regenerateId() { m_id = QUuid::createUuid().toString(QUuid::WithoutBraces); }

QJsonObject Position::toJson() const {
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["shortName"] = m_shortName;
    json["delay"] = m_delay;
    json["pan"] = m_pan;

    if (!m_buses.isEmpty()) {
        QJsonArray busArray;
        for (int bus : m_buses) {
            busArray.append(bus);
        }
        json["buses"] = busArray;
    }
    return json;
}

Position Position::fromJson(const QJsonObject& json) {
    Position position;
    position.m_id = json["id"].toString();
    if (position.m_id.isEmpty()) {
        position.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    position.m_name = json["name"].toString();
    position.m_shortName = json["shortName"].toString();
    position.m_delay = json["delay"].toDouble();
    position.m_pan = json["pan"].toDouble();

    if (json.contains("buses")) {
        for (const QJsonValue& val : json["buses"].toArray()) {
            position.m_buses.append(val.toInt());
        }
    }
    return position;
}

// --- PositionLibrary ---

PositionLibrary::PositionLibrary(QObject* parent) : QObject(parent) {}

int PositionLibrary::indexOf(const QString& id) const {
    for (int i = 0; i < m_positions.size(); ++i) {
        if (m_positions[i].id() == id) {
            return i;
        }
    }
    return -1;
}

bool PositionLibrary::contains(const QString& id) const { return indexOf(id) >= 0; }

QString PositionLibrary::addPosition(const Position& position) {
    m_positions.append(position);
    emit positionAdded(position.id());
    emit changed();
    return position.id();
}

void PositionLibrary::updatePosition(const Position& position) {
    int idx = indexOf(position.id());
    if (idx >= 0) {
        m_positions[idx] = position;
        emit positionChanged(position.id());
        emit changed();
    } else {
        addPosition(position);
    }
}

bool PositionLibrary::removePosition(const QString& id) {
    int idx = indexOf(id);
    if (idx < 0) {
        return false;
    }
    m_positions.removeAt(idx);
    emit positionRemoved(id);
    emit changed();
    return true;
}

void PositionLibrary::setPositions(const QList<Position>& positions) {
    m_positions = positions;
    emit changed();
}

void PositionLibrary::clear() {
    m_positions.clear();
    emit libraryCleared();
    emit changed();
}

std::optional<Position> PositionLibrary::position(const QString& id) const {
    int idx = indexOf(id);
    if (idx < 0) {
        return std::nullopt;
    }
    return m_positions[idx];
}

QJsonObject PositionLibrary::toJson() const {
    QJsonObject json;
    QJsonArray arr;
    for (const Position& position : m_positions) {
        arr.append(position.toJson());
    }
    json["positions"] = arr;
    return json;
}

PositionLibrary* PositionLibrary::fromJson(const QJsonObject& json, QObject* parent) {
    auto* library = new PositionLibrary(parent);
    library->loadFromJson(json);
    return library;
}

void PositionLibrary::loadFromJson(const QJsonObject& json) {
    m_positions.clear();
    for (const QJsonValue& val : json["positions"].toArray()) {
        m_positions.append(Position::fromJson(val.toObject()));
    }
}

} // namespace OpenMix
