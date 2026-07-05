#include "core/Actor.h"
#include "core/ActorProfile.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/Ensemble.h"
#include "core/Show.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestActorProfiles : public QObject {
    Q_OBJECT

  private slots:
    // --- VoiceData ---------------------------------------------------------
    void voiceData_roundTrips_onlySetFields() {
        VoiceData v;
        v.gainDb = 12.5;
        v.hpfOn = true;
        v.hpfFreq = 80.0;
        v.eqOn = true;
        v.eqBands.append(EqBand{2, true, 1, 2500.0, -3.0, 1.4});
        v.dynOn = true;
        v.dynThreshold = -18.0;
        v.dynRatio = 3.0;

        const QJsonObject json = v.toJson();
        QVERIFY(!json.contains("dynRelease")); // unset stays absent

        VoiceData r = VoiceData::fromJson(json);
        QVERIFY(r.gainDb.has_value());
        QCOMPARE(*r.gainDb, 12.5);
        QCOMPARE(*r.hpfFreq, 80.0);
        QCOMPARE(r.eqBands.size(), 1);
        QCOMPARE(r.eqBands[0].freq, 2500.0);
        QCOMPARE(*r.dynRatio, 3.0);
        QVERIFY(!r.dynRelease.has_value());
    }

    void voiceData_empty_isEmpty() {
        VoiceData v;
        QVERIFY(v.isEmpty());
        v.gainDb = 0.0; // 0 is still "set"
        QVERIFY(!v.isEmpty());
    }

    // --- ActorProfile (main + backup) -------------------------------------
    void actorProfile_mainAndBackup_roundTrip() {
        ActorProfile p;
        p.main().gainDb = 6.0;
        p.backup().gainDb = -90.0;

        ActorProfile r = ActorProfile::fromJson(p.toJson());
        QCOMPARE(*r.main().gainDb, 6.0);
        QCOMPARE(*r.backup().gainDb, -90.0);
    }

    // --- Actor -------------------------------------------------------------
    void actor_storesProfilesPerSlot_roundTrip() {
        Actor a("Alice", 5);
        ActorProfile main;
        main.main().gainDb = 3.0;
        a.setProfile("Main", main);

        QVERIFY(a.hasProfile("Main"));
        QCOMPARE(a.profileSlots().size(), 1);

        Actor r = Actor::fromJson(a.toJson());
        QCOMPARE(r.name(), QString("Alice"));
        QCOMPARE(r.channel(), 5);
        QCOMPARE(r.id(), a.id());
        QVERIFY(r.hasProfile("Main"));
        QCOMPARE(*r.profile("Main").main().gainDb, 3.0);
    }

    void actor_roles_roundTrip() {
        Actor a("Alice", 5);
        a.setRoles({"Cosette", "Singer #1"});

        const QJsonObject json = a.toJson();
        const QJsonArray rolesArr = json["roles"].toArray();
        QCOMPARE(rolesArr.size(), 2);
        QCOMPARE(rolesArr[0].toString(), QString("Cosette"));
        QCOMPARE(rolesArr[1].toString(), QString("Singer #1"));
        // legacy key carries the primary role for pre-1.9 builds
        QCOMPARE(json["role"].toString(), QString("Cosette"));

        Actor r = Actor::fromJson(json);
        QCOMPARE(r.roles(), QStringList({"Cosette", "Singer #1"}));
    }

    void actor_useRoleName_roundTripAndDisplayName() {
        Actor a("WL01", 17);
        a.setRoles({"Valjean"});
        QCOMPARE(a.displayName(), QString("WL01"));
        QCOMPARE(a.secondaryName(), QString("Valjean"));

        a.setUseRoleName(true);
        QCOMPARE(a.displayName(), QString("Valjean"));
        QCOMPARE(a.secondaryName(), QString("WL01"));

        Actor r = Actor::fromJson(a.toJson());
        QVERIFY(r.useRoleName());
        QCOMPARE(r.displayName(), QString("Valjean"));

        // no roles to prefer: falls back to the actor name
        Actor bare("Bob", 4);
        bare.setUseRoleName(true);
        QCOMPARE(bare.displayName(), QString("Bob"));
        QVERIFY(bare.secondaryName().isEmpty());

        // identical name and role never render as "Barry (Barry)"
        Actor same("Barry", 1);
        same.setRoles({"Barry"});
        QVERIFY(same.secondaryName().isEmpty());

        // default flag stays out of the file
        QVERIFY(!Actor("Alice", 5).toJson().contains("useRoleName"));
    }

    void actor_emptyRoles_omittedFromJson() {
        Actor a("Alice", 5);
        QVERIFY(!a.toJson().contains("roles"));
        QVERIFY(!a.toJson().contains("role"));

        // absent keys load as empty (pre-1.8 shows)
        Actor r = Actor::fromJson(a.toJson());
        QVERIFY(r.roles().isEmpty());
    }

    void actor_legacySingleRole_loadsIntoRoles() {
        // the exact shape a 1.8 build writes: single "role" string, no "roles"
        QJsonObject legacy;
        legacy["id"] = "some-id";
        legacy["name"] = "Alice";
        legacy["role"] = "Cosette";
        legacy["channel"] = 5;

        Actor r = Actor::fromJson(legacy);
        QCOMPARE(r.roles(), QStringList({"Cosette"}));

        // when both keys are present, "roles" wins
        legacy["roles"] = QJsonArray({"Eponine", "Dancer"});
        Actor both = Actor::fromJson(legacy);
        QCOMPARE(both.roles(), QStringList({"Eponine", "Dancer"}));
    }

    void actor_roleHelpers() {
        Actor a("Alice", 5);
        QVERIFY(a.primaryRole().isEmpty());
        QVERIFY(a.rolesDisplay().isEmpty());
        QVERIFY(a.matchedRole("Cosette").isEmpty());

        a.setRoles({"Cosette", "Singer #1"});
        QCOMPARE(a.primaryRole(), QString("Cosette"));
        QCOMPARE(a.rolesDisplay(), QString("Cosette, Singer #1"));
        QCOMPARE(a.matchedRole("singer #1"), QString("Singer #1")); // ci hit, stored casing
        QVERIFY(a.matchedRole("Eponine").isEmpty());
        QVERIFY(a.matchedRole("").isEmpty());
    }

    // --- ActorProfileLibrary: lookup + backup -----------------------------
    void library_voiceFor_resolvesActiveActor() {
        ActorProfileLibrary lib;
        Actor a("Bob", 7);
        ActorProfile p;
        p.main().gainDb = 4.0;
        p.backup().gainDb = -80.0;
        a.setProfile("Main", p);
        lib.addActor(a);

        auto main = lib.voiceFor(7, "Main");
        QVERIFY(main.has_value());
        QCOMPARE(*main->gainDb, 4.0);

        // flip channel to backup mic -> backup voice resolves
        lib.setBackup(7, true);
        auto backup = lib.voiceFor(7, "Main");
        QVERIFY(backup.has_value());
        QCOMPARE(*backup->gainDb, -80.0);

        // unknown channel / slot -> nullopt
        QVERIFY(!lib.voiceFor(8, "Main").has_value());
        QVERIFY(!lib.voiceFor(7, "Solo").has_value());
    }

    void library_actorForChannel_prefersLowestOrderActive() {
        ActorProfileLibrary lib;
        Actor primary("Lead", 3);
        primary.setOrder(1);
        Actor understudy("Understudy", 3);
        understudy.setOrder(2);
        lib.addActor(understudy);
        lib.addActor(primary);

        QCOMPARE(lib.actorForChannel(3)->name(), QString("Lead"));

        // deactivate lead -> understudy takes the channel
        primary.setActive(false);
        lib.updateActor(primary.id(), primary);
        QCOMPARE(lib.actorForChannel(3)->name(), QString("Understudy"));
    }

    void library_resolveActor_roleBeatsName_caseInsensitive_trimmed() {
        ActorProfileLibrary lib;
        Actor alice("Alice", 5);
        alice.setRoles({"Cosette"});
        lib.addActor(alice);
        // an actor literally named "Cosette" must lose to the role match
        lib.addActor(Actor("Cosette", 6));

        QCOMPARE(lib.resolveActor("cosette")->channel(), 5);
        QCOMPARE(lib.resolveActor("  ALICE  ")->channel(), 5);
        QCOMPARE(lib.resolveActor("alice")->name(), QString("Alice"));
    }

    void library_resolveActor_matchesAnyRole() {
        ActorProfileLibrary lib;
        Actor alice("Alice", 5);
        alice.setRoles({"Cosette", "Singer #1"});
        lib.addActor(alice);
        // a non-primary role still beats an actor named like it
        lib.addActor(Actor("Singer #1", 6));

        QCOMPARE(lib.resolveActor("singer #1")->channel(), 5);
        QCOMPARE(lib.resolveActor("Cosette")->channel(), 5);
    }

    void library_resolveActor_prefersActiveThenLowestOrder() {
        ActorProfileLibrary lib;
        Actor lead("Lead", 3);
        lead.setRoles({"Evan"});
        lead.setOrder(1);
        Actor understudy("Understudy", 4);
        understudy.setRoles({"Evan"});
        understudy.setOrder(2);
        lib.addActor(understudy);
        lib.addActor(lead);

        QCOMPARE(lib.resolveActor("Evan")->name(), QString("Lead"));

        lead.setActive(false);
        lib.updateActor(lead.id(), lead);
        QCOMPARE(lib.resolveActor("Evan")->name(), QString("Understudy"));
    }

    void library_resolveActor_noMatchReturnsNull() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Alice", 5));
        QVERIFY(lib.resolveActor("Bob") == nullptr);
        QVERIFY(lib.resolveActor("") == nullptr);
        QVERIFY(lib.resolveActor("   ") == nullptr);
    }

    void library_resolveChannels_roleMatchesAllActiveActors() {
        ActorProfileLibrary lib;
        auto addWithRole = [&lib](const QString& name, int ch, bool active) {
            Actor a(name, ch);
            a.setRoles({"Nuns"});
            a.setActive(active);
            lib.addActor(a);
        };
        addWithRole("Alice", 5, true);
        addWithRole("Beth", 7, true);
        addWithRole("Cara", 3, true);
        addWithRole("Dora", 9, false); // inactive cover excluded

        QCOMPARE(lib.resolveChannels("nuns"), QList<int>({3, 5, 7}));
    }

    void library_resolveChannels_ensembleName_returnsMemberChannels() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Alice", 5));
        EnsembleLibrary ensembles;
        Ensemble band("Band");
        band.setChannels({10, 11});
        ensembles.addEnsemble(band);

        QCOMPARE(lib.resolveChannels("band", &ensembles), QList<int>({10, 11}));
        // without the ensemble library the name resolves to nothing
        QCOMPARE(lib.resolveChannels("band"), QList<int>());
    }

    void library_resolveChannels_rolePrecedesEnsemble() {
        ActorProfileLibrary lib;
        Actor kid("Kid Lead", 2);
        kid.setRoles({"Kids"});
        lib.addActor(kid);
        EnsembleLibrary ensembles;
        Ensemble kids("Kids");
        kids.setChannels({20, 21});
        ensembles.addEnsemble(kids);

        QCOMPARE(lib.resolveChannels("Kids", &ensembles), QList<int>({2}));
    }

    void library_resolveChannels_ensemblePrecedesActorName() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Band", 4)); // actor literally named like the group
        EnsembleLibrary ensembles;
        Ensemble band("Band");
        band.setChannels({15, 16});
        ensembles.addEnsemble(band);

        QCOMPARE(lib.resolveChannels("Band", &ensembles), QList<int>({15, 16}));
    }

    void library_resolveChannels_allInactive_fallsBackToSingleBest() {
        ActorProfileLibrary lib;
        Actor lead("Lead", 3);
        lead.setRoles({"Evan"});
        lead.setOrder(1);
        lead.setActive(false);
        Actor cover("Cover", 4);
        cover.setRoles({"Evan"});
        cover.setOrder(2);
        cover.setActive(false);
        lib.addActor(cover);
        lib.addActor(lead);

        // resolveActor's preference (lowest order among equally-inactive) holds
        QCOMPARE(lib.resolveChannels("Evan"), QList<int>({3}));
    }

    void library_resolveChannels_skipsUnassignedChannel_andDedups() {
        ActorProfileLibrary lib;
        Actor unset("Unset", 0); // no channel assigned yet
        unset.setRoles({"Chorus"});
        lib.addActor(unset);
        Actor a("Alto", 6);
        a.setRoles({"Chorus"});
        lib.addActor(a);
        Actor b("Bass", 6); // shares the channel
        b.setRoles({"Chorus"});
        lib.addActor(b);

        QCOMPARE(lib.resolveChannels("Chorus"), QList<int>({6}));
        QCOMPARE(lib.resolveChannels(""), QList<int>());
        QCOMPARE(lib.resolveChannels("   "), QList<int>());
    }

    void library_completionCandidates_includesEnsembleNames() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Alice", 5));
        EnsembleLibrary ensembles;
        ensembles.addEnsemble(Ensemble("Band"));
        ensembles.addEnsemble(Ensemble("alice")); // ci-duplicate of the actor

        QCOMPARE(lib.completionCandidates(&ensembles), QStringList({"Alice", "Band"}));
    }

    void library_completionCandidates_dedupSkipsEmpty() {
        ActorProfileLibrary lib;
        Actor alice("Alice", 5);
        alice.setRoles({"Cosette", "Singer #1"}); // every role becomes a candidate
        lib.addActor(alice);
        lib.addActor(Actor("Bob", 6)); // no role
        Actor dup("COSETTE", 7);       // ci-duplicate of the role
        lib.addActor(dup);

        const QStringList candidates = lib.completionCandidates();
        QCOMPARE(candidates, QStringList({"Alice", "Bob", "Cosette", "Singer #1"}));
    }

    void library_emitsSignals() {
        ActorProfileLibrary lib;
        QSignalSpy added(&lib, &ActorProfileLibrary::actorAdded);
        QSignalSpy changed(&lib, &ActorProfileLibrary::changed);

        Actor a("Cleo", 1);
        lib.addActor(a);

        QCOMPARE(added.count(), 1);
        QVERIFY(changed.count() >= 1);
    }

    void library_roundTrips() {
        ActorProfileLibrary lib;
        lib.setProfileSlots({"Main", "Solo"});
        Actor a("Dana", 2);
        ActorProfile p;
        p.main().eqOn = true;
        p.main().eqBands.append(EqBand{1, true, 0, 100.0, 2.0, 1.0});
        a.setProfile("Solo", p);
        lib.addActor(a);
        lib.setBackup(2, true);

        ActorProfileLibrary other;
        other.loadFromJson(lib.toJson());

        QCOMPARE(other.profileSlots(), QStringList({"Main", "Solo"}));
        QCOMPARE(other.actorCount(), 1);
        QVERIFY(other.isBackup(2));
        const Actor* loaded = other.actorForChannel(2);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QString("Dana"));
        QVERIFY(loaded->profile("Solo").main().eqOn.value_or(false));
    }

    // --- Cue: channelProfiles / channelLevels -----------------------------
    void cue_channelProfilesAndLevels_roundTrip() {
        Cue cue(1.0, "Top");
        cue.setChannelProfile(5, "Main");
        cue.setChannelProfile(6, "Solo");
        cue.setChannelLevel(5, 0.75);

        Cue r = Cue::fromJson(cue.toJson());
        QCOMPARE(r.channelProfiles().value(5), QString("Main"));
        QCOMPARE(r.channelProfiles().value(6), QString("Solo"));
        QCOMPARE(r.channelLevels().value(5), 0.75);
        QVERIFY(!r.channelLevels().contains(6));
    }

    // --- Show: 1.2 persistence + 1.1 back-compat --------------------------
    void show_persistsActors_andMarksDirty() {
        Show show;
        QSignalSpy spy(&show, &Show::modifiedChanged);
        Actor a("Eve", 4);
        a.setRoles({"Elle"});
        show.actorProfileLibrary()->addActor(a);
        QVERIFY(show.isModified()); // library change propagates dirty
        QVERIFY(spy.count() >= 1);

        const QJsonObject json = show.toJson();
        QCOMPARE(json["version"].toString(), QString("1.10"));

        Show loaded;
        loaded.fromJson(json);
        QCOMPARE(loaded.actorProfileLibrary()->actorCount(), 1);
        QCOMPARE(loaded.actorProfileLibrary()->actorForChannel(4)->name(), QString("Eve"));
        QCOMPARE(loaded.actorProfileLibrary()->actorForChannel(4)->roles(), QStringList({"Elle"}));
    }

    void show_loadsLegacy_1_1_withoutActors() {
        // a 1.1 document has no "actors" key; load must succeed with an empty library
        QJsonObject legacy;
        legacy["version"] = "1.1";
        legacy["name"] = "Legacy";
        legacy["cues"] = QJsonArray();

        Show show;
        show.actorProfileLibrary()->addActor(Actor("Stale", 9)); // pre-existing state
        show.fromJson(legacy);

        QCOMPARE(show.name(), QString("Legacy"));
        QCOMPARE(show.actorProfileLibrary()->actorCount(), 0); // cleared on load
        QVERIFY(!show.isModified());
    }
};

QTEST_MAIN(TestActorProfiles)
#include "test_actor_profiles.moc"
