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

void DCAMapping::clearChannelFromAllDCAs(int channel) {
    for (auto it = m_channelAssignments.begin(); it != m_channelAssignments.end();) {
        if (it.value().removeAll(channel) > 0) {
            if (it.value().isEmpty()) {
                it = m_channelAssignments.erase(it);
                continue;
            }
        }
        ++it;
    }
}

QList<int> DCAMapping::channelsForDCA(int dca) const { return m_channelAssignments.value(dca); }

int DCAMapping::dcaForChannel(int channel) const {
    for (auto it = m_channelAssignments.constBegin(); it != m_channelAssignments.constEnd(); ++it) {
        if (it.value().contains(channel)) {
            return it.key();
        }
    }
    return -1;
}

bool DCAMapping::isChannelAssigned(int channel) const { return dcaForChannel(channel) >= 0; }

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
    for (auto it = m_busAssignments.begin(); it != m_busAssignments.end();) {
        if (it.value().removeAll(bus) > 0) {
            if (it.value().isEmpty()) {
                it = m_busAssignments.erase(it);
                continue;
            }
        }
        ++it;
    }
}

QList<int> DCAMapping::busesForDCA(int dca) const { return m_busAssignments.value(dca); }

int DCAMapping::dcaForBus(int bus) const {
    for (auto it = m_busAssignments.constBegin(); it != m_busAssignments.constEnd(); ++it) {
        if (it.value().contains(bus)) {
            return it.key();
        }
    }
    return -1;
}

bool DCAMapping::isBusAssigned(int bus) const { return dcaForBus(bus) >= 0; }

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
    for (auto it = m_channelAssignments.constBegin(); it != m_channelAssignments.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            dcas.insert(it.key());
        }
    }
    for (auto it = m_busAssignments.constBegin(); it != m_busAssignments.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            dcas.insert(it.key());
        }
    }
    return dcas;
}

QJsonObject DCAMapping::toJson() const {
    QJsonObject json;

    QJsonObject channelsObj;
    for (auto it = m_channelAssignments.constBegin(); it != m_channelAssignments.constEnd(); ++it) {
        QJsonArray channels;
        for (int ch : it.value()) {
            channels.append(ch);
        }
        channelsObj[QString::number(it.key())] = channels;
    }
    json["channels"] = channelsObj;

    QJsonObject busesObj;
    for (auto it = m_busAssignments.constBegin(); it != m_busAssignments.constEnd(); ++it) {
        QJsonArray buses;
        for (int bus : it.value()) {
            buses.append(bus);
        }
        busesObj[QString::number(it.key())] = buses;
    }
    json["buses"] = busesObj;

    if (!m_busNames.isEmpty()) {
        QJsonObject busNamesObj;
        for (auto it = m_busNames.constBegin(); it != m_busNames.constEnd(); ++it) {
            busNamesObj[QString::number(it.key())] = it.value();
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

    QJsonObject channelsObj = json["channels"].toObject();
    for (auto it = channelsObj.constBegin(); it != channelsObj.constEnd(); ++it) {
        int dca = it.key().toInt();
        QJsonArray channels = it.value().toArray();
        QList<int> channelList;
        for (const QJsonValue& val : channels) {
            channelList.append(val.toInt());
        }
        if (!channelList.isEmpty()) {
            m_channelAssignments[dca] = channelList;
        }
    }

    QJsonObject busesObj = json["buses"].toObject();
    for (auto it = busesObj.constBegin(); it != busesObj.constEnd(); ++it) {
        int dca = it.key().toInt();
        QJsonArray buses = it.value().toArray();
        QList<int> busList;
        for (const QJsonValue& val : buses) {
            busList.append(val.toInt());
        }
        if (!busList.isEmpty()) {
            m_busAssignments[dca] = busList;
        }
    }

    QJsonObject busNamesObj = json["busNames"].toObject();
    for (auto it = busNamesObj.constBegin(); it != busNamesObj.constEnd(); ++it) {
        int bus = it.key().toInt();
        QString name = it.value().toString();
        if (bus > 0 && !name.isEmpty()) {
            m_busNames[bus] = name;
        }
    }
}

} // namespace OpenMix
