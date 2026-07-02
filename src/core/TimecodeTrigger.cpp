#include "TimecodeTrigger.h"
#include <QJsonArray>
#include <QUuid>

namespace OpenMix {

QJsonObject TimecodeTrigger::toJson() const {
    QJsonObject json;
    json["id"] = id;
    json["hours"] = hours;
    json["minutes"] = minutes;
    json["seconds"] = seconds;
    json["frames"] = frames;
    json["cueNumber"] = cueNumber;
    json["enabled"] = enabled;
    return json;
}

TimecodeTrigger TimecodeTrigger::fromJson(const QJsonObject& json) {
    TimecodeTrigger t;
    t.id = json["id"].toString();
    if (t.id.isEmpty()) {
        t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    t.hours = json["hours"].toInt();
    t.minutes = json["minutes"].toInt();
    t.seconds = json["seconds"].toInt();
    t.frames = json["frames"].toInt();
    t.cueNumber = json["cueNumber"].toDouble();
    t.enabled = json["enabled"].toBool(true);
    return t;
}

// --- TimecodeTriggerList ---

TimecodeTriggerList::TimecodeTriggerList(QObject* parent) : QObject(parent) {}

QString TimecodeTriggerList::addTrigger(int h, int m, int s, int f, double cueNumber) {
    TimecodeTrigger trigger;
    trigger.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    trigger.hours = h;
    trigger.minutes = m;
    trigger.seconds = s;
    trigger.frames = f;
    trigger.cueNumber = cueNumber;
    addTrigger(trigger);
    return trigger.id;
}

void TimecodeTriggerList::addTrigger(const TimecodeTrigger& trigger) {
    m_triggers.append(trigger);
    emit triggerAdded(trigger.id);
}

bool TimecodeTriggerList::removeTrigger(const QString& id) {
    for (int i = 0; i < m_triggers.size(); ++i) {
        if (m_triggers[i].id == id) {
            m_triggers.removeAt(i);
            m_fired.remove(id);
            emit triggerRemoved(id);
            return true;
        }
    }
    return false;
}

void TimecodeTriggerList::setTriggerEnabled(const QString& id, bool enabled) {
    for (TimecodeTrigger& trigger : m_triggers) {
        if (trigger.id == id) {
            trigger.enabled = enabled;
            return;
        }
    }
}

void TimecodeTriggerList::setTriggers(const QList<TimecodeTrigger>& triggers) {
    m_triggers = triggers;
    m_fired.clear();
}

void TimecodeTriggerList::clear() {
    m_triggers.clear();
    m_fired.clear();
    m_lastOrder = -1;
    emit triggersCleared();
}

void TimecodeTriggerList::resetArming() {
    m_fired.clear();
    m_lastOrder = -1;
}

void TimecodeTriggerList::onTimecode(int hours, int minutes, int seconds, int frames) {
    const long current = order(hours, minutes, seconds, frames);

    for (const TimecodeTrigger& trigger : m_triggers) {
        if (!trigger.enabled) {
            continue;
        }
        const long target = order(trigger.hours, trigger.minutes, trigger.seconds, trigger.frames);

        if (current < target) {
            // timecode is before the trigger point: (re-)arm it
            m_fired.remove(trigger.id);
        } else if (!m_fired.contains(trigger.id) && m_lastOrder < target) {
            // crossed from before the point to at/after it: fire once
            m_fired.insert(trigger.id);
            emit triggerFired(trigger.cueNumber, trigger.id);
        }
    }

    m_lastOrder = current;
}

QJsonObject TimecodeTriggerList::toJson() const {
    QJsonObject json;
    QJsonArray arr;
    for (const TimecodeTrigger& trigger : m_triggers) {
        arr.append(trigger.toJson());
    }
    json["triggers"] = arr;
    return json;
}

void TimecodeTriggerList::loadFromJson(const QJsonObject& json) {
    m_triggers.clear();
    m_fired.clear();
    m_lastOrder = -1;
    for (const QJsonValue& val : json["triggers"].toArray()) {
        m_triggers.append(TimecodeTrigger::fromJson(val.toObject()));
    }
}

} // namespace OpenMix
