#include "core/Actor.h"
#include "core/ActorProfile.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
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

    void actor_role_roundTrip() {
        Actor a("Alice", 5);
        a.setRole("Cosette");

        const QJsonObject json = a.toJson();
        QCOMPARE(json["role"].toString(), QString("Cosette"));

        Actor r = Actor::fromJson(json);
        QCOMPARE(r.role(), QString("Cosette"));
    }

    void actor_emptyRole_omittedFromJson() {
        Actor a("Alice", 5);
        QVERIFY(!a.toJson().contains("role"));

        // absent key loads as empty (pre-1.8 shows)
        Actor r = Actor::fromJson(a.toJson());
        QVERIFY(r.role().isEmpty());
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
        alice.setRole("Cosette");
        lib.addActor(alice);
        // an actor literally named "Cosette" must lose to the role match
        lib.addActor(Actor("Cosette", 6));

        QCOMPARE(lib.resolveActor("cosette")->channel(), 5);
        QCOMPARE(lib.resolveActor("  ALICE  ")->channel(), 5);
        QCOMPARE(lib.resolveActor("alice")->name(), QString("Alice"));
    }

    void library_resolveActor_prefersActiveThenLowestOrder() {
        ActorProfileLibrary lib;
        Actor lead("Lead", 3);
        lead.setRole("Evan");
        lead.setOrder(1);
        Actor understudy("Understudy", 4);
        understudy.setRole("Evan");
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

    void library_completionCandidates_dedupSkipsEmpty() {
        ActorProfileLibrary lib;
        Actor alice("Alice", 5);
        alice.setRole("Cosette");
        lib.addActor(alice);
        lib.addActor(Actor("Bob", 6)); // no role
        Actor dup("COSETTE", 7);       // ci-duplicate of the role
        lib.addActor(dup);

        const QStringList candidates = lib.completionCandidates();
        QCOMPARE(candidates, QStringList({"Alice", "Bob", "Cosette"}));
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
        a.setRole("Elle");
        show.actorProfileLibrary()->addActor(a);
        QVERIFY(show.isModified()); // library change propagates dirty
        QVERIFY(spy.count() >= 1);

        const QJsonObject json = show.toJson();
        QCOMPARE(json["version"].toString(), QString("1.8"));

        Show loaded;
        loaded.fromJson(json);
        QCOMPARE(loaded.actorProfileLibrary()->actorCount(), 1);
        QCOMPARE(loaded.actorProfileLibrary()->actorForChannel(4)->name(), QString("Eve"));
        QCOMPARE(loaded.actorProfileLibrary()->actorForChannel(4)->role(), QString("Elle"));
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
