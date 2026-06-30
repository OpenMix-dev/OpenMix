#pragma once

#include "ActorProfile.h"
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

namespace OpenMix {

// A cast member assigned to a console input channel. Holds one ActorProfile per
// profile slot (e.g. "Main", "Solo"); the active slot is chosen per-cue. Profiles
// follow the actor, so swapping in an understudy swaps the whole voice set.
class Actor {
  public:
    Actor();
    explicit Actor(const QString& name, int channel = 0);

    [[nodiscard]] QString id() const { return m_id; }
    void setId(const QString& id) { m_id = id; }
    void regenerateId();

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    [[nodiscard]] int channel() const noexcept { return m_channel; }
    void setChannel(int channel) { m_channel = channel; }

    [[nodiscard]] int order() const noexcept { return m_order; }
    void setOrder(int order) { m_order = order; }

    [[nodiscard]] bool active() const noexcept { return m_active; }
    void setActive(bool active) { m_active = active; }

    // per-slot profiles
    [[nodiscard]] bool hasProfile(const QString& slot) const { return m_profiles.contains(slot); }
    [[nodiscard]] ActorProfile profile(const QString& slot) const { return m_profiles.value(slot); }
    void setProfile(const QString& slot, const ActorProfile& profile) { m_profiles[slot] = profile; }
    void removeProfile(const QString& slot) { m_profiles.remove(slot); }
    [[nodiscard]] QStringList profileSlots() const { return m_profiles.keys(); }

    QJsonObject toJson() const;
    [[nodiscard]] static Actor fromJson(const QJsonObject& json);

  private:
    QString m_id;
    QString m_name;
    int m_channel = 0;
    int m_order = 0;
    bool m_active = true;
    QMap<QString, ActorProfile> m_profiles; // slot -> profile
};

} // namespace OpenMix
