#include "ActorProfileLibrary.h"
#include "Ensemble.h"

#include <QJsonArray>

#include <algorithm>

namespace OpenMix {

ActorProfileLibrary::ActorProfileLibrary(QObject* parent) : QObject(parent) {}

void ActorProfileLibrary::setProfileSlots(const QStringList& slotList) {
    m_slots = slotList.isEmpty() ? QStringList{DEFAULT_SLOT} : slotList;
    emit changed();
}

void ActorProfileLibrary::addSlot(const QString& slot) {
    if (slot.isEmpty() || m_slots.contains(slot))
        return;
    m_slots.append(slot);
    emit changed();
}

void ActorProfileLibrary::removeSlot(const QString& slot) {
    if (m_slots.removeAll(slot) > 0) {
        if (m_slots.isEmpty())
            m_slots.append(DEFAULT_SLOT);
        emit changed();
    }
}

int ActorProfileLibrary::indexOfActor(const QString& id) const {
    for (int i = 0; i < m_actors.size(); ++i) {
        if (m_actors[i].id() == id)
            return i;
    }
    return -1;
}

const Actor* ActorProfileLibrary::actorById(const QString& id) const {
    int idx = indexOfActor(id);
    return idx >= 0 ? &m_actors[idx] : nullptr;
}

const Actor* ActorProfileLibrary::actorForChannel(int channel) const {
    // the active actor on this channel; if several are assigned, lowest order wins
    const Actor* best = nullptr;
    for (const Actor& a : m_actors) {
        if (a.channel() != channel || !a.active())
            continue;
        if (!best || a.order() < best->order())
            best = &a;
    }
    return best;
}

const Actor* ActorProfileLibrary::resolveActor(const QString& text) const {
    const QString needle = text.trimmed();
    if (needle.isEmpty())
        return nullptr;

    auto better = [](const Actor* candidate, const Actor* best) {
        if (!best)
            return true;
        if (candidate->active() != best->active())
            return candidate->active();
        return candidate->order() < best->order();
    };

    const Actor* byRole = nullptr;
    const Actor* byName = nullptr;
    for (const Actor& a : m_actors) {
        if (!a.matchedRole(needle).isEmpty() && better(&a, byRole))
            byRole = &a;
        if (needle.compare(a.name(), Qt::CaseInsensitive) == 0 && better(&a, byName))
            byName = &a;
    }
    return byRole ? byRole : byName;
}

QList<int> ActorProfileLibrary::resolveChannels(const QString& text,
                                                const EnsembleLibrary* ensembles) const {
    const QString needle = text.trimmed();
    if (needle.isEmpty())
        return {};

    QList<int> channels;
    auto collectActive = [&](auto&& matches) {
        for (const Actor& a : m_actors) {
            if (a.active() && a.channel() >= 1 && matches(a))
                channels.append(a.channel());
        }
    };

    collectActive([&](const Actor& a) { return !a.matchedRole(needle).isEmpty(); });

    if (channels.isEmpty() && ensembles) {
        for (const Ensemble& e : ensembles->ensembles()) {
            if (needle.compare(e.name(), Qt::CaseInsensitive) == 0) {
                channels = e.channels(); // already sorted/unique/positive
                break;
            }
        }
    }

    if (channels.isEmpty())
        collectActive(
            [&](const Actor& a) { return needle.compare(a.name(), Qt::CaseInsensitive) == 0; });

    if (channels.isEmpty()) {
        if (const Actor* fallback = resolveActor(needle); fallback && fallback->channel() >= 1)
            channels.append(fallback->channel());
    }

    std::sort(channels.begin(), channels.end());
    channels.erase(std::unique(channels.begin(), channels.end()), channels.end());
    return channels;
}

QStringList ActorProfileLibrary::completionCandidates(const EnsembleLibrary* ensembles) const {
    QStringList candidates;
    auto add = [&candidates](const QString& text) {
        if (!text.isEmpty() && !candidates.contains(text, Qt::CaseInsensitive))
            candidates.append(text);
    };
    for (const Actor& a : m_actors) {
        for (const QString& role : a.roles())
            add(role);
        add(a.name());
    }
    if (ensembles) {
        for (const Ensemble& e : ensembles->ensembles())
            add(e.name());
    }
    candidates.sort(Qt::CaseInsensitive);
    return candidates;
}

void ActorProfileLibrary::addActor(const Actor& actor) {
    m_actors.append(actor);
    emit actorAdded(actor.id());
    emit changed();
}

void ActorProfileLibrary::updateActor(const QString& id, const Actor& actor) {
    int idx = indexOfActor(id);
    if (idx < 0)
        return;
    m_actors[idx] = actor;
    emit actorModified(id);
    emit changed();
}

void ActorProfileLibrary::removeActor(const QString& id) {
    int idx = indexOfActor(id);
    if (idx < 0)
        return;
    m_actors.removeAt(idx);
    emit actorRemoved(id);
    emit changed();
}

void ActorProfileLibrary::clear() {
    m_actors.clear();
    m_backupChannels.clear();
    m_slots = QStringList{DEFAULT_SLOT};
    emit changed();
}

void ActorProfileLibrary::setBackup(int channel, bool on) {
    bool changedState = on ? !m_backupChannels.contains(channel) : m_backupChannels.contains(channel);
    if (!changedState)
        return;
    if (on)
        m_backupChannels.insert(channel);
    else
        m_backupChannels.remove(channel);
    emit changed();
}

std::optional<VoiceData> ActorProfileLibrary::voiceFor(int channel, const QString& slot) const {
    const Actor* actor = actorForChannel(channel);
    if (!actor || !actor->hasProfile(slot))
        return std::nullopt;
    const ActorProfile profile = actor->profile(slot);
    const VoiceData& voice = isBackup(channel) ? profile.backup() : profile.main();
    if (voice.isEmpty())
        return std::nullopt;
    return voice;
}

QJsonObject ActorProfileLibrary::toJson() const {
    QJsonObject json;

    QJsonArray slotsArr;
    for (const QString& s : m_slots)
        slotsArr.append(s);
    json["slots"] = slotsArr;

    QJsonArray actorsArr;
    for (const Actor& a : m_actors)
        actorsArr.append(a.toJson());
    json["actors"] = actorsArr;

    if (!m_backupChannels.isEmpty()) {
        QJsonArray backupArr;
        for (int ch : m_backupChannels)
            backupArr.append(ch);
        json["backupChannels"] = backupArr;
    }

    return json;
}

void ActorProfileLibrary::loadFromJson(const QJsonObject& json) {
    m_actors.clear();
    m_backupChannels.clear();
    m_slots.clear();

    for (const QJsonValue& val : json["slots"].toArray())
        m_slots.append(val.toString());
    if (m_slots.isEmpty())
        m_slots.append(DEFAULT_SLOT);

    for (const QJsonValue& val : json["actors"].toArray())
        m_actors.append(Actor::fromJson(val.toObject()));

    for (const QJsonValue& val : json["backupChannels"].toArray())
        m_backupChannels.insert(val.toInt());

    emit changed();
}

} // namespace OpenMix
