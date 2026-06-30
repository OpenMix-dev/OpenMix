#pragma once

#include "core/Actor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// Serialization helpers for exchanging actors between shows: "actor values"
// (the whole cast) and "actor groups" (a named subset), mirroring TheatreMix's
// import/export and load/save-group actions in its Actor Setup dialog.
//
// Header-only and free of any widget/Qt-GUI dependency so it links into headless
// unit tests. The on-disk format wraps the same per-actor JSON the core model
// already round-trips (Actor::toJson / fromJson), plus the show's profile slots.
namespace OpenMix::ActorGroupIo {

inline constexpr int FORMAT_VERSION = 1;
inline constexpr const char* MAGIC_KEY = "openmix.actorGroup";

// Serialize a set of actors plus the profile-slot names into a portable document.
[[nodiscard]] inline QJsonObject toJson(const QList<Actor>& actors, const QStringList& slotNames) {
    QJsonObject doc;
    doc[MAGIC_KEY] = FORMAT_VERSION;

    QJsonArray slotsArr;
    for (const QString& s : slotNames)
        slotsArr.append(s);
    doc["slots"] = slotsArr;

    QJsonArray actorsArr;
    for (const Actor& a : actors)
        actorsArr.append(a.toJson());
    doc["actors"] = actorsArr;

    return doc;
}

// True if the document looks like an actor-group export (has the magic key or an
// "actors" array). Lenient so a raw library export ("actors" only) also loads.
[[nodiscard]] inline bool isActorGroup(const QJsonObject& doc) {
    return doc.contains(MAGIC_KEY) || doc.contains("actors");
}

// The profile-slot names stored in the document (empty if none).
[[nodiscard]] inline QStringList slotsFromJson(const QJsonObject& doc) {
    QStringList slotNames;
    for (const QJsonValue& v : doc["slots"].toArray())
        slotNames.append(v.toString());
    return slotNames;
}

// Reconstruct the actors. When regenerateIds is true (the default for import, to
// avoid colliding with actors already in the target library) each actor gets a
// fresh id; pass false to preserve ids for an exact round-trip.
[[nodiscard]] inline QList<Actor> actorsFromJson(const QJsonObject& doc,
                                                 bool regenerateIds = false) {
    QList<Actor> actors;
    for (const QJsonValue& v : doc["actors"].toArray()) {
        Actor a = Actor::fromJson(v.toObject());
        if (regenerateIds)
            a.regenerateId();
        actors.append(a);
    }
    return actors;
}

} // namespace OpenMix::ActorGroupIo
