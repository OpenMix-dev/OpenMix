#include "Actor.h"

#include <QJsonObject>
#include <QUuid>

namespace OpenMix {

Actor::Actor() : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

Actor::Actor(const QString& name, int channel)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_name(name), m_channel(channel) {}

void Actor::regenerateId() { m_id = QUuid::createUuid().toString(QUuid::WithoutBraces); }

QJsonObject Actor::toJson() const {
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["channel"] = m_channel;
    json["order"] = m_order;
    json["active"] = m_active;

    if (!m_profiles.isEmpty()) {
        QJsonObject profilesObj;
        for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
            QJsonObject p = it.value().toJson();
            if (!p.isEmpty())
                profilesObj[it.key()] = p;
        }
        if (!profilesObj.isEmpty())
            json["profiles"] = profilesObj;
    }
    return json;
}

Actor Actor::fromJson(const QJsonObject& json) {
    Actor actor;
    actor.m_id = json["id"].toString();
    if (actor.m_id.isEmpty())
        actor.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    actor.m_name = json["name"].toString();
    actor.m_channel = json["channel"].toInt();
    actor.m_order = json["order"].toInt();
    actor.m_active = json["active"].toBool(true);

    const QJsonObject profilesObj = json["profiles"].toObject();
    for (auto it = profilesObj.constBegin(); it != profilesObj.constEnd(); ++it)
        actor.m_profiles[it.key()] = ActorProfile::fromJson(it.value().toObject());

    return actor;
}

} // namespace OpenMix
