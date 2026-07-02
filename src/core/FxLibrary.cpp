#include "FxLibrary.h"
#include <QJsonArray>

namespace OpenMix {

QJsonObject FxUnit::toJson() const {
    QJsonObject json;
    json["index"] = index;
    json["name"] = name;
    return json;
}

FxUnit FxUnit::fromJson(const QJsonObject& json) {
    FxUnit unit;
    unit.index = json.value("index").toInt();
    unit.name = json.value("name").toString();
    return unit;
}

FxLibrary::FxLibrary(QObject* parent) : QObject(parent) {}

void FxLibrary::setUnit(int index, const QString& name) {
    if (m_units.value(index) == name && m_units.contains(index))
        return;
    m_units[index] = name;
    emit changed();
}

void FxLibrary::removeUnit(int index) {
    bool touched = m_units.remove(index) > 0;
    // drop the unit from any channel assignment
    for (auto it = m_assignments.begin(); it != m_assignments.end(); ++it) {
        if (it.value().removeAll(index) > 0)
            touched = true;
    }
    if (m_defaultFx == index) {
        m_defaultFx = -1;
        touched = true;
    }
    if (touched)
        emit changed();
}

QString FxLibrary::unitName(int index) const {
    return m_units.value(index);
}

QList<FxUnit> FxLibrary::units() const {
    QList<FxUnit> list;
    for (auto it = m_units.begin(); it != m_units.end(); ++it)
        list.append({it.key(), it.value()});
    // QMap iterates keys ascending, so the list is already sorted by index
    return list;
}

void FxLibrary::setChannelAssignment(int channel, const QList<int>& fxUnits) {
    if (fxUnits.isEmpty()) {
        clearChannelAssignment(channel);
        return;
    }
    m_assignments[channel] = fxUnits;
    emit changed();
}

QList<int> FxLibrary::channelAssignment(int channel) const {
    return m_assignments.value(channel);
}

void FxLibrary::clearChannelAssignment(int channel) {
    if (m_assignments.remove(channel) > 0)
        emit changed();
}

void FxLibrary::setDefaultFx(int index) {
    if (m_defaultFx == index)
        return;
    m_defaultFx = index;
    emit changed();
}

bool FxLibrary::isEmpty() const {
    return m_units.isEmpty() && m_assignments.isEmpty() && m_defaultFx < 0;
}

void FxLibrary::clear() {
    if (isEmpty())
        return;
    m_units.clear();
    m_assignments.clear();
    m_defaultFx = -1;
    emit changed();
}

QJsonObject FxLibrary::toJson() const {
    QJsonObject json;

    QJsonArray unitsArray;
    for (const FxUnit& unit : units())
        unitsArray.append(unit.toJson());
    json["units"] = unitsArray;

    QJsonObject assignsObj;
    for (auto it = m_assignments.begin(); it != m_assignments.end(); ++it) {
        QJsonArray units;
        for (int fx : it.value())
            units.append(fx);
        assignsObj[QString::number(it.key())] = units;
    }
    json["assignments"] = assignsObj;
    json["defaultFx"] = m_defaultFx;
    return json;
}

void FxLibrary::loadFromJson(const QJsonObject& json) {
    m_units.clear();
    m_assignments.clear();

    const QJsonArray unitsArray = json.value("units").toArray();
    for (const QJsonValue& val : unitsArray) {
        const FxUnit unit = FxUnit::fromJson(val.toObject());
        m_units[unit.index] = unit.name;
    }

    const QJsonObject assignsObj = json.value("assignments").toObject();
    for (auto it = assignsObj.begin(); it != assignsObj.end(); ++it) {
        QList<int> units;
        for (const QJsonValue& val : it.value().toArray())
            units.append(val.toInt());
        if (!units.isEmpty())
            m_assignments[it.key().toInt()] = units;
    }

    m_defaultFx = json.value("defaultFx").toInt(-1);
    emit changed();
}

} // namespace OpenMix
