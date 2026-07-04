#include "core/Actor.h"
#include "core/ActorProfile.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/FadeEngine.h"
#include "core/PlaybackEngine.h"
#include "protocol/LoopbackProtocol.h"
#include "protocol/MixerCapabilities.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {
LoopbackProtocol* makeConnectedLoopback(QObject* parent) {
    auto* mixer = new LoopbackProtocol(MixerCapabilities::forConsole(ConsoleType::X32), parent);
    [[maybe_unused]] const bool ok = mixer->connect("loopback", 0); // Connected synchronously
    Q_ASSERT(ok);
    return mixer;
}
} // namespace

class TestPlaybackProfiles : public QObject {
    Q_OBJECT

  private slots:
    void firingCue_appliesActorVoiceAndLevel() {
        ActorProfileLibrary lib;
        Actor a("Lead", 5);
        ActorProfile p;
        p.main().gainDb = 6.0;
        p.main().hpfOn = true;
        p.main().hpfFreq = 90.0;
        p.main().eqOn = true;
        p.main().eqBands.append(EqBand{2, true, 2, 2500.0, -2.0, 1.5});
        a.setProfile("Main", p);
        lib.addActor(a);

        CueList cues;
        Cue cue(1.0, "Top");
        cue.setType(CueType::Snapshot);
        cue.setChannelProfile(5, "Main");
        cue.setChannelLevel(5, 0.5);
        cues.addCue(cue);

        LoopbackProtocol* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setActorLibrary(&lib);
        engine.setMixer(mixer);
        engine.executeCue(0);

        const QStringList calls = mixer->recordedCalls();
        QVERIFY2(calls.contains("preamp:ch=5:gain=6"), qPrintable(calls.join(" | ")));
        QVERIFY(calls.contains("hpf:ch=5:on=1:freq=90"));
        QVERIFY(calls.contains("eqon:ch=5:on=1"));
        QVERIFY(calls.contains("fader:ch=5:level=0.5"));

        bool sawEqBand = false;
        for (const QString& c : calls) {
            if (c.startsWith("eqband:ch=5:band=2"))
                sawEqBand = true;
        }
        QVERIFY(sawEqBand);
    }

    void firingCue_backupChannel_appliesBackupVoice() {
        ActorProfileLibrary lib;
        Actor a("Lead", 7);
        ActorProfile p;
        p.main().gainDb = 3.0;
        p.backup().gainDb = -10.0;
        a.setProfile("Main", p);
        lib.addActor(a);
        lib.setBackup(7, true); // channel on its spare mic

        CueList cues;
        Cue cue(1.0, "Top");
        cue.setType(CueType::Snapshot);
        cue.setChannelProfile(7, "Main");
        cues.addCue(cue);

        LoopbackProtocol* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setActorLibrary(&lib);
        engine.setMixer(mixer);
        engine.executeCue(0);

        QVERIFY(mixer->recordedCalls().contains("preamp:ch=7:gain=-10"));
        QVERIFY(!mixer->recordedCalls().contains("preamp:ch=7:gain=3"));
    }

    void firingCue_noActorOnChannel_appliesNoVoice() {
        ActorProfileLibrary lib; // empty cast
        CueList cues;
        Cue cue(1.0, "Top");
        cue.setType(CueType::Snapshot);
        cue.setChannelProfile(3, "Main");
        cues.addCue(cue);

        LoopbackProtocol* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setActorLibrary(&lib);
        engine.setMixer(mixer);
        engine.executeCue(0);

        for (const QString& c : mixer->recordedCalls())
            QVERIFY(!c.startsWith("preamp:ch=3"));
    }

    void instantCue_setsChannelLevelImmediately() {
        CueList cues;
        Cue a(1.0, "A");
        a.setType(CueType::Snapshot);
        a.setChannelLevel(5, 0.6);
        cues.addCue(a);

        LoopbackProtocol* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.executeCue(0);

        QVERIFY(mixer->recordedCalls().contains("fader:ch=5:level=0.6"));
        QVERIFY(!engine.fadeEngine()->isActive());
    }

    void fadeCue_fadesChannelLevelFromPriorValue() {
        CueList cues;
        Cue a(1.0, "A");
        a.setType(CueType::Snapshot);
        a.setChannelLevel(5, 0.0);
        Cue b(2.0, "B");
        b.setType(CueType::Snapshot);
        b.setChannelLevel(5, 1.0);
        b.setFadeTime(2.0);
        b.setFadeCurve(FadeCurve::Linear);
        cues.addCue(a);
        cues.addCue(b);

        LoopbackProtocol* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);

        engine.executeCue(0); // A: instant, sets applied level to 0
        mixer->clearRecordedCalls();

        engine.executeCue(1); // B: fade from 0 -> applies 'from' (0) now, not target yet
        QVERIFY(mixer->recordedCalls().contains("fader:ch=5:level=0"));
        QVERIFY(!mixer->recordedCalls().contains("fader:ch=5:level=1"));
        QVERIFY(engine.fadeEngine()->isActive());

        engine.fadeEngine()->advance(2000.0); // run the fade to completion
        QVERIFY(mixer->recordedCalls().contains("fader:ch=5:level=1"));
        QVERIFY(!engine.fadeEngine()->isActive());
    }

    void verifyCue_emitsLanded_whenParamsMatch() {
        CueList cues;
        Cue a(1.0, "A");
        a.setType(CueType::Snapshot);
        a.setParameter("/ch/01/mix/fader", 0.5);
        cues.addCue(a);

        LoopbackProtocol* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.setVerifyCues(true);

        QSignalSpy landed(&engine, &PlaybackEngine::cueLanded);
        QSignalSpy drifted(&engine, &PlaybackEngine::cueDrifted);

        engine.executeCue(0);

        QVERIFY(landed.wait(1000)); // loopback echoes the value back asynchronously
        QCOMPARE(landed.count(), 1);
        QCOMPARE(drifted.count(), 0);
    }
};

QTEST_MAIN(TestPlaybackProfiles)
#include "test_playback_profiles.moc"
