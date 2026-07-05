#pragma once

#include "Actor.h"
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

namespace OpenMix {

class EnsembleLibrary;

// Owns the show's cast: actors (each assigned to a channel), the set of profile
// slots, and which channels are currently on their backup/spare voice. Resolves
// the concrete voice to apply for a given channel + slot. Lives on the Show.
class ActorProfileLibrary : public QObject {
    Q_OBJECT

  public:
    static constexpr const char* DEFAULT_SLOT = "Main";

    explicit ActorProfileLibrary(QObject* parent = nullptr);

    // profile slots (categories of voice), default {"Main"}. NOTE: not named slots()
    // because `slots` is a Qt moc keyword macro.
    [[nodiscard]] QStringList profileSlots() const { return m_slots; }
    void setProfileSlots(const QStringList& slotList);
    void addSlot(const QString& slot);
    void removeSlot(const QString& slot);

    // actors
    [[nodiscard]] const QList<Actor>& actors() const { return m_actors; }
    [[nodiscard]] int actorCount() const { return m_actors.size(); }
    [[nodiscard]] const Actor* actorById(const QString& id) const;
    [[nodiscard]] const Actor* actorForChannel(int channel) const; // active actor on channel
    // resolve free text to an actor: case-insensitive match against any of the
    // actor's roles first, then actor name; among duplicates prefer active,
    // then lowest order
    [[nodiscard]] const Actor* resolveActor(const QString& text) const;
    // resolve free text to the input channels it names, in tiers:
    //   1. every ACTIVE actor holding the role (case-insensitive)
    //   2. else the identically-named ensemble's member channels
    //   3. else every ACTIVE actor with that name
    //   4. else the single resolveActor() fallback (covers inactive understudies)
    // sorted, deduplicated, excludes unassigned channels (< 1)
    [[nodiscard]] QList<int> resolveChannels(const QString& text,
                                             const EnsembleLibrary* ensembles = nullptr) const;
    // roles + actor names (+ ensemble names) for autocomplete (non-empty,
    // deduplicated, sorted)
    [[nodiscard]] QStringList completionCandidates(const EnsembleLibrary* ensembles = nullptr) const;
    void addActor(const Actor& actor);
    void updateActor(const QString& id, const Actor& actor);
    void removeActor(const QString& id);
    void clear();

    // backup (spare-mic) channels: resolve to the backup voice instead of main
    [[nodiscard]] bool isBackup(int channel) const { return m_backupChannels.contains(channel); }
    void setBackup(int channel, bool on);
    [[nodiscard]] QSet<int> backupChannels() const { return m_backupChannels; }

    // resolve the voice to apply for channel+slot, honoring backup state.
    // returns nullopt if no active actor / no stored voice.
    [[nodiscard]] std::optional<VoiceData> voiceFor(int channel, const QString& slot) const;

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed();
    void actorAdded(const QString& id);
    void actorModified(const QString& id);
    void actorRemoved(const QString& id);

  private:
    [[nodiscard]] int indexOfActor(const QString& id) const;

    QStringList m_slots{DEFAULT_SLOT};
    QList<Actor> m_actors;
    QSet<int> m_backupChannels;
};

} // namespace OpenMix
