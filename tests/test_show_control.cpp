#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {

// records every driver call as a readable string so fire behaviour can be
// asserted without real hardware. Always reports Connected once connect() runs.
class RecordingMixer : public MixerProtocol {
  public:
    explicit RecordingMixer(QObject* parent = nullptr) : MixerProtocol(parent) {}

    QString protocolName() const override { return "recording"; }
    QString protocolDescription() const override { return "recording mock"; }

    bool connect(const QString&, int) override {
        m_connected = true;
        return true;
    }
    void disconnect() override { m_connected = false; }
    bool isConnected() const override { return m_connected; }
    QString connectionStatus() const override { return "connected"; }
    ConnectionState connectionState() const override {
        return m_connected ? ConnectionState::Connected : ConnectionState::Disconnected;
    }

    void sendParameter(const QString& path, const QVariant& value) override {
        calls << QString("send:%1=%2").arg(path, value.toString());
    }
    QVariant getParameter(const QString&) override { return {}; }
    void requestParameter(const QString&) override {}
    void requestParameterAsync(const QString& path, ParameterCallback cb) override {
        if (cb)
            cb(path, QVariant(), false);
    }

    void recallSnapshot(const Cue&) override {}
    void recallScene(int scene) override { calls << QString("scene:%1").arg(scene); }
    void recallSnippet(int snippet) override { calls << QString("snippet:%1").arg(snippet); }

    void setChannelFader(int channel, double level) override {
        calls << QString("fader:ch=%1:level=%2").arg(channel).arg(level);
    }

    void refresh() override {}
    int latencyMs() const override { return 0; }

    QStringList calls;

  private:
    bool m_connected = false;
};

RecordingMixer* makeConnectedMixer(QObject* parent) {
    auto* mixer = new RecordingMixer(parent);
    mixer->connect("mock", 0);
    return mixer;
}

} // namespace

class TestShowControl : public QObject {
    Q_OBJECT

  private slots:
    // --- serialization ---

    void cue_fxMutes_roundtrip() {
        Cue cue(1.0, "A");
        cue.setFxMute(1, true);
        cue.setFxMute(4, false);

        Cue loaded = Cue::fromJson(cue.toJson());
        QCOMPARE(loaded.fxMutes().size(), 2);
        QCOMPARE(loaded.fxMutes().value(1), true);
        QCOMPARE(loaded.fxMutes().value(4), false);
    }

    void cue_snippets_roundtrip() {
        Cue cue;
        cue.setSnippets({3, 7, 9});

        Cue loaded = Cue::fromJson(cue.toJson());
        QCOMPARE(loaded.snippets(), QList<int>({3, 7, 9}));
    }

    void cue_color_roundtrip() {
        Cue cue;
        cue.setColor("#ff8800");

        Cue loaded = Cue::fromJson(cue.toJson());
        QCOMPARE(loaded.color(), QString("#ff8800"));
    }

    void cue_skip_roundtrip() {
        Cue cue;
        cue.setSkip(true);
        QVERIFY(Cue::fromJson(cue.toJson()).skip());

        Cue defaultCue;
        QVERIFY(!Cue::fromJson(defaultCue.toJson()).skip());
    }

    void show_channelGangs_roundtrip() {
        Show show;
        show.setChannelGangs({qMakePair(1, 2), qMakePair(3, 4)});

        Show loaded;
        loaded.fromJson(show.toJson());
        QCOMPARE(loaded.channelGangs().size(), 2);
        QCOMPARE(loaded.channelGangs().at(0), qMakePair(1, 2));
        QCOMPARE(loaded.channelGangs().at(1), qMakePair(3, 4));
    }

    // --- on-fire playback ---

    void fire_sendsFxMutes() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cue.setFxMute(1, true);
        cue.setFxMute(2, false);
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.executeCue(0);

        QVERIFY2(mixer->calls.contains("send:/fx/1/mute=1"), qPrintable(mixer->calls.join(" | ")));
        QVERIFY(mixer->calls.contains("send:/fx/2/mute=0"));
    }

    void fire_recallsSnippets() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cue.addSnippet(3);
        cue.addSnippet(7);
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.executeCue(0);

        QVERIFY(mixer->calls.contains("snippet:3"));
        QVERIFY(mixer->calls.contains("snippet:7"));
    }

    void fire_gangMirrorsLevelToPartner() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cue.setChannelLevel(5, 0.4);
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.setChannelGangs({qMakePair(5, 6)});
        engine.executeCue(0);

        QVERIFY2(mixer->calls.contains("fader:ch=5:level=0.4"), qPrintable(mixer->calls.join(" | ")));
        QVERIFY(mixer->calls.contains("fader:ch=6:level=0.4"));
    }

    void fire_gangDoesNotOverrideExplicitPartner() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cue.setChannelLevel(5, 0.4);
        cue.setChannelLevel(6, 0.9); // partner has its own explicit level
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.setChannelGangs({qMakePair(5, 6)});
        engine.executeCue(0);

        QVERIFY(mixer->calls.contains("fader:ch=6:level=0.9"));
        QVERIFY(!mixer->calls.contains("fader:ch=6:level=0.4"));
    }

    // --- skip + check mode ---

    void advanceStandby_stepsOverSkippedCue() {
        CueList cues;
        cues.addCue(Cue(1.0, "A"));
        Cue skipped(2.0, "B");
        skipped.setSkip(true);
        cues.addCue(skipped);
        cues.addCue(Cue(3.0, "C"));

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues); // standby -> 0
        engine.setMixer(mixer);

        engine.go(); // fire A, advance past skipped B to C
        QCOMPARE(engine.currentCueIndex(), 0);
        QCOMPARE(engine.standbyCueIndex(), 2);
    }

    void next_stepsOverSkippedCue() {
        CueList cues;
        cues.addCue(Cue(1.0, "A"));
        Cue skipped(2.0, "B");
        skipped.setSkip(true);
        cues.addCue(skipped);
        cues.addCue(Cue(3.0, "C"));

        PlaybackEngine engine;
        engine.setCueList(&cues); // standby -> 0
        engine.next();
        QCOMPARE(engine.standbyCueIndex(), 2);
    }

    void checkMode_holdsGoOnCurrentCue() {
        CueList cues;
        cues.addCue(Cue(1.0, "A"));
        cues.addCue(Cue(2.0, "B"));
        cues.addCue(Cue(3.0, "C"));

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues); // standby -> 0
        engine.setMixer(mixer);
        engine.setCheckMode(true);

        engine.go();
        QCOMPARE(engine.currentCueIndex(), 0);
        QCOMPARE(engine.standbyCueIndex(), 0); // held, not advanced

        engine.go();
        QCOMPARE(engine.currentCueIndex(), 0); // re-fires the same cue

        engine.setCheckMode(false);
        engine.go(); // now advances normally
        QCOMPARE(engine.standbyCueIndex(), 1);
    }

    void checkMode_emitsSignalOnChange() {
        PlaybackEngine engine;
        QSignalSpy spy(&engine, &PlaybackEngine::checkModeChanged);

        engine.setCheckMode(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);

        engine.setCheckMode(true); // no change
        QCOMPARE(spy.count(), 1);

        engine.setCheckMode(false);
        QCOMPARE(spy.count(), 2);
        QVERIFY(!engine.checkMode());
    }
};

QTEST_MAIN(TestShowControl)
#include "test_show_control.moc"
