#include "DCAMapping.h"
#include <QJsonArray>

namespace OpenMix {

DCAMapping::DCAMapping(QObject* parent) : QObject(parent) {}

void DCAMapping::assignChannelToDCA(int channel, int dca) {
    clearChannelFromAllDCAs(channel);

    if (!m_channelAssignments[dca].contains(channel)) {
        m_channelAssignments[dca].append(channel);
        emit channelAssignmentChanged(channel, dca);
    }
}

void DCAMapping::removeChannelFromDCA(int channel, int dca) {
    if (m_channelAssignments.contains(dca)) {
        if (m_channelAssignments[dca].removeAll(channel) > 0) {
            if (m_channelAssignments[dca].isEmpty()) {
                m_channelAssignments.remove(dca);
            }
            emit channelAssignmentChanged(channel, -1);
        }
    }
}

void DCAMapping::clearFromAllDCAs(QMap<int, QList<int>>& map, int id) {
    for (auto it = map.begin(); it != map.end();) {
        if (it.value().removeAll(id) > 0) {
            if (it.value().isEmpty()) {
                it = map.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void DCAMapping::clearChannelFromAllDCAs(int channel) {
    clearFromAllDCAs(m_channelAssignments, channel);
}

QList<int> DCAMapping::channelsForDCA(int dca) const { return m_channelAssignments.value(dca); }

std::optional<int> DCAMapping::dcaForChannel(int channel) const {
    for (const auto& [dca, channels] : m_channelAssignments.asKeyValueRange()) {
        if (channels.contains(channel)) {
            return dca;
        }
    }
    return std::nullopt;
}

bool DCAMapping::isChannelAssigned(int channel) const { return dcaForChannel(channel).has_value(); }

void DCAMapping::assignBusToDCA(int bus, int dca) {
    clearBusFromAllDCAs(bus);

    if (!m_busAssignments[dca].contains(bus)) {
        m_busAssignments[dca].append(bus);
        emit busAssignmentChanged(bus, dca);
    }
}

void DCAMapping::removeBusFromDCA(int bus, int dca) {
    if (m_busAssignments.contains(dca)) {
        if (m_busAssignments[dca].removeAll(bus) > 0) {
            if (m_busAssignments[dca].isEmpty()) {
                m_busAssignments.remove(dca);
            }
            emit busAssignmentChanged(bus, -1);
        }
    }
}

void DCAMapping::clearBusFromAllDCAs(int bus) {
    clearFromAllDCAs(m_busAssignments, bus);
}

QList<int> DCAMapping::busesForDCA(int dca) const { return m_busAssignments.value(dca); }

std::optional<int> DCAMapping::dcaForBus(int bus) const {
    for (const auto& [dca, buses] : m_busAssignments.asKeyValueRange()) {
        if (buses.contains(bus)) {
            return dca;
        }
    }
    return std::nullopt;
}

bool DCAMapping::isBusAssigned(int bus) const { return dcaForBus(bus).has_value(); }

void DCAMapping::setBusName(int bus, const QString& name) {
    if (name.isEmpty()) {
        m_busNames.remove(bus);
    } else {
        m_busNames[bus] = name;
    }
    emit busNameChanged(bus, name);
}

QString DCAMapping::busName(int bus) const { return m_busNames.value(bus, QString()); }

void DCAMapping::clear() {
    m_channelAssignments.clear();
    m_busAssignments.clear();
    emit mappingCleared();
}

void DCAMapping::setChannelAssignments(const QMap<int, QList<int>>& assignments) {
    m_channelAssignments = assignments;
}

void DCAMapping::setBusAssignments(const QMap<int, QList<int>>& assignments) {
    m_busAssignments = assignments;
}

QSet<int> DCAMapping::assignedDCAs() const {
    QSet<int> dcas;
    for (const auto& [dca, channels] : m_channelAssignments.asKeyValueRange()) {
        if (!channels.isEmpty())
            dcas.insert(dca);
    }
    for (const auto& [dca, buses] : m_busAssignments.asKeyValueRange()) {
        if (!buses.isEmpty())
            dcas.insert(dca);
    }
    return dcas;
}

QJsonObject DCAMapping::mappingToJsonObject(const QMap<int, QList<int>>& map) {
    QJsonObject obj;
    for (const auto& [dca, items] : map.asKeyValueRange()) {
        QJsonArray arr;
        for (int item : items)
            arr.append(item);
        obj[QString::number(dca)] = arr;
    }
    return obj;
}

QMap<int, QList<int>> DCAMapping::jsonObjectToMapping(const QJsonObject& obj) {
    QMap<int, QList<int>> result;
    for (const auto& [key, val] : obj.asKeyValueRange()) {
        QList<int> items;
        for (const QJsonValue& v : val.toArray())
            items.append(v.toInt());
        if (!items.isEmpty())
            result[key.toString().toInt()] = items;
    }
    return result;
}

QJsonObject DCAMapping::toJson() const {
    QJsonObject json;
    json["channels"] = mappingToJsonObject(m_channelAssignments);
    json["buses"]    = mappingToJsonObject(m_busAssignments);

    if (!m_busNames.isEmpty()) {
        QJsonObject busNamesObj;
        for (const auto& [bus, name] : m_busNames.asKeyValueRange()) {
            busNamesObj[QString::number(bus)] = name;
        }
        json["busNames"] = busNamesObj;
    }

    return json;
}

DCAMapping* DCAMapping::fromJson(const QJsonObject& json, QObject* parent) {
    DCAMapping* mapping = new DCAMapping(parent);
    mapping->loadFromJson(json);
    return mapping;
}

void DCAMapping::loadFromJson(const QJsonObject& json) {
    m_channelAssignments.clear();
    m_busAssignments.clear();
    m_busNames.clear();

    m_channelAssignments = jsonObjectToMapping(json["channels"].toObject());
    m_busAssignments     = jsonObjectToMapping(json["buses"].toObject());

    for (const auto& [key, val] : json["busNames"].toObject().asKeyValueRange()) {
        int bus = key.toString().toInt();
        QString name = val.toString();
        if (bus > 0 && !name.isEmpty())
            m_busNames[bus] = name;
    }
}

} // namespace OpenMix
