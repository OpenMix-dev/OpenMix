#pragma once

#include "ActorProfile.h"
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

namespace OpenMix {

// A cast member assigned to a console input channel, optionally tagged with the
// roles (characters/parts) they play. Holds one ActorProfile per profile slot
// (e.g. "Main", "Solo"); the active slot is chosen per-cue. Profiles follow the
// actor, so swapping in an understudy swaps the whole voice set.
class Actor {
  public:
    Actor();
    explicit Actor(const QString& name, int channel = 0);

    [[nodiscard]] QString id() const { return m_id; }
    void setId(const QString& id) { m_id = id; }
    void regenerateId();

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    [[nodiscard]] QStringList roles() const { return m_roles; }
    void setRoles(const QStringList& roles) { m_roles = roles; }
    [[nodiscard]] QString primaryRole() const { return m_roles.value(0); }
    [[nodiscard]] QString rolesDisplay() const { return m_roles.join(QStringLiteral(", ")); }
    // the stored role case-insensitively equal to text, or empty when none
    [[nodiscard]] QString matchedRole(const QString& text) const;

    // label the channel with the primary role instead of the actor name
    // (for channels that aren't tied to one person, e.g. shared mic packs)
    [[nodiscard]] bool useRoleName() const noexcept { return m_useRoleName; }
    void setUseRoleName(bool use) { m_useRoleName = use; }

    // primary label for this actor's channel (console scribble strips and
    // channel displays): the role when preferred and present, else the name
    [[nodiscard]] QString displayName() const {
        return (m_useRoleName && !m_roles.isEmpty()) ? m_roles.first() : m_name;
    }
    // the complementary label shown in parentheses next to displayName():
    // normally the role (the matched one when given), the actor name when the
    // role is the display name; empty when redundant or absent
    [[nodiscard]] QString secondaryName(const QString& matchedRole = QString()) const;

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
    QStringList m_roles; // characters/parts; empty when unused
    int m_channel = 0;
    int m_order = 0;
    bool m_active = true;
    bool m_useRoleName = false; // channel labelled by role instead of name
    QMap<QString, ActorProfile> m_profiles; // slot -> profile
};

} // namespace OpenMix
