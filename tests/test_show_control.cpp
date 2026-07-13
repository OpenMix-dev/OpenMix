#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"
#include <QHash>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <optional>

using namespace OpenMix;

namespace {

// records every driver call as a readable string so fire behavior can be
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

    void setChannelMute(int channel, bool muted) override {
        calls << QString("mute:ch=%1:%2").arg(channel).arg(muted ? 1 : 0);
    }

    void setChannelDcaMask(int channel, quint32 mask) override {
        chMasks[channel] = mask;
        calls << QString("dcamask:ch=%1:mask=%2").arg(channel).arg(mask);
    }
    void setBusDcaMask(int bus, quint32 mask) override {
        busMasks[bus] = mask;
        calls << QString("dcamask:bus=%1:mask=%2").arg(bus).arg(mask);
    }
    std::optional<quint32> readChannelDcaMask(int channel) override {
        if (chMasks.contains(channel))
            return chMasks.value(channel);
        return std::nullopt;
    }
    std::optional<quint32> readBusDcaMask(int bus) override {
        if (busMasks.contains(bus))
            return busMasks.value(bus);
        return std::nullopt;
    }

    void refresh() override {}
    int latencyMs() const override { return 0; }

    QStringList calls;
    QHash<int, quint32> chMasks;
    QHash<int, quint32> busMasks;

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

    void show_consoleToggles_roundtrip() {
        Show show;
        show.setDimDcaFaders(true);
        show.setSelectOnSpill(true);
        show.setMuteDcaUnassign(true);
        show.setSuppressBackupSwitch(true);
        show.setDesigner("Jane Doe");

        Show loaded;
        loaded.fromJson(show.toJson());
        QVERIFY(loaded.dimDcaFaders());
        QVERIFY(loaded.selectOnSpill());
        QVERIFY(loaded.muteDcaUnassign());
        QVERIFY(loaded.suppressBackupSwitch());
        QCOMPARE(loaded.designer(), QString("Jane Doe"));

        // defaults are all off
        Show fresh;
        Show freshLoaded;
        freshLoaded.fromJson(fresh.toJson());
        QVERIFY(!freshLoaded.dimDcaFaders());
        QVERIFY(!freshLoaded.muteDcaUnassign());
    }

    void show_gangMeta_roundtrip() {
        Show show;
        show.setChannelGangs({qMakePair(1, 2), qMakePair(3, 4)});
        show.setGangName(0, "Vocals L/R");
        show.setGangColor(0, "#ff0000");
        show.setGangName(1, "FX");

        Show loaded;
        loaded.fromJson(show.toJson());
        QCOMPARE(loaded.gangName(0), QString("Vocals L/R"));
        QCOMPARE(loaded.gangColor(0), QString("#ff0000"));
        QCOMPARE(loaded.gangName(1), QString("FX"));
        // gang metadata stays aligned with the gang list
        QCOMPARE(loaded.channelGangs().size(), 2);
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

    // --- channel mute buttons ---

    void toggleChannelMute_flipsAndDispatches() {
        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setMixer(mixer);

        QSignalSpy spy(&engine, &PlaybackEngine::channelMuteChanged);

        engine.toggleChannelMute(5);
        QVERIFY(engine.isChannelMuted(5));
        QVERIFY2(mixer->calls.contains("mute:ch=5:1"), qPrintable(mixer->calls.join(" | ")));

        engine.toggleChannelMute(5);
        QVERIFY(!engine.isChannelMuted(5));
        QVERIFY(mixer->calls.contains("mute:ch=5:0"));

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    // --- DCA console-behavior toggles on fire ---

    void fire_muteDcaUnassign_clearsMembership() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        DCAOverride ov;
        ov.mute = true;
        cue.setTargetedDCAs({2});
        cue.setDCAOverride(2, ov);
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        mixer->chMasks[5] = 0x2; // ch5 rides DCA2 (bit 1)
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.setMuteDcaUnassign(true);
        engine.executeCue(0);

        QVERIFY2(mixer->calls.contains("send:/dca/2/mute=1"), qPrintable(mixer->calls.join(" | ")));
        QVERIFY(mixer->calls.contains("dcamask:ch=5:mask=0"));
    }

    void fire_dimDcaFaders_sendsFaderDim() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        DCAOverride ov;
        ov.mute = true;
        cue.setTargetedDCAs({2});
        cue.setDCAOverride(2, ov);
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.setDimDcaFaders(true);
        engine.executeCue(0);

        QVERIFY2(mixer->calls.contains("send:/dca/2/fader=0"), qPrintable(mixer->calls.join(" | ")));
    }

    void fire_dcaToggles_offByDefault() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        DCAOverride ov;
        ov.mute = true;
        cue.setTargetedDCAs({2});
        cue.setDCAOverride(2, ov);
        cues.addCue(cue);

        RecordingMixer* mixer = makeConnectedMixer(this);
        mixer->chMasks[5] = 0x2; // ch5 rides DCA2
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.executeCue(0);

        // only the plain mute is sent; no unassign/dim side effects
        QVERIFY(mixer->calls.contains("send:/dca/2/mute=1"));
        QVERIFY(!mixer->calls.contains("dcamask:ch=5:mask=0"));
        QVERIFY(!mixer->calls.contains("send:/dca/2/fader=0"));
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

    // --- list reset (new show / show load) ---

    void listClear_resetsPlaybackIndices() {
        CueList cues;
        cues.addCue(Cue(1.0, "A"));
        cues.addCue(Cue(2.0, "B"));
        cues.addCue(Cue(3.0, "C"));

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues); // standby -> 0
        engine.setMixer(mixer);
        engine.go(); // current 0, standby 1

        QSignalSpy currentSpy(&engine, &PlaybackEngine::currentCueChanged);
        QSignalSpy standbySpy(&engine, &PlaybackEngine::standbyCueChanged);

        cues.clear();

        QCOMPARE(engine.currentCueIndex(), -1);
        QCOMPARE(engine.standbyCueIndex(), -1);
        QCOMPARE(engine.currentCue(), nullptr);
        QCOMPARE(engine.standbyCue(), nullptr);
        QCOMPARE(engine.state(), PlaybackState::Stopped);
        QCOMPARE(currentSpy.count(), 1);
        QCOMPARE(currentSpy.at(0).at(0).toInt(), -1);
        QCOMPARE(standbySpy.count(), 1);
        QCOMPARE(standbySpy.at(0).at(0).toInt(), -1);

        engine.go(); // no-op on an empty list
        QCOMPARE(engine.currentCueIndex(), -1);
        QCOMPARE(engine.standbyCueIndex(), -1);
    }

    void listLoad_rearmsStandbyAtFirstCue() {
        CueList cues;
        cues.addCue(Cue(1.0, "A"));
        cues.addCue(Cue(2.0, "B"));
        cues.addCue(Cue(3.0, "C"));

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.go();
        engine.go(); // current 1, standby 2

        CueList smaller;
        smaller.addCue(Cue(9.0, "Z"));
        cues.fromJson(smaller.toJson()); // the Open Show path

        QCOMPARE(cues.count(), 1);
        QCOMPARE(engine.currentCueIndex(), -1);
        QCOMPARE(engine.standbyCueIndex(), 0);
        QCOMPARE(engine.state(), PlaybackState::Stopped);
    }

    void listLoad_emptyShow_leavesNothingArmed() {
        CueList cues;
        cues.addCue(Cue(1.0, "A"));

        PlaybackEngine engine;
        engine.setCueList(&cues); // standby -> 0

        cues.fromJson(QJsonArray());

        QCOMPARE(engine.standbyCueIndex(), -1);
        QCOMPARE(engine.standbyCue(), nullptr);
    }

    void listClear_cancelsRunningFades() {
        CueList cues;
        Cue a(1.0, "A");
        a.setType(CueType::Snapshot);
        a.setChannelLevel(5, 0.0);
        Cue b(2.0, "B");
        b.setType(CueType::Snapshot);
        b.setChannelLevel(5, 1.0);
        b.setFadeTime(1.0);
        cues.addCue(a);
        cues.addCue(b);

        RecordingMixer* mixer = makeConnectedMixer(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        engine.executeCue(0); // instant, seeds the applied level
        engine.executeCue(1); // fades 0 -> 1
        QVERIFY(engine.fadeEngine()->isActive());

        cues.clear();
        QVERIFY(!engine.fadeEngine()->isActive());
    }
};

QTEST_MAIN(TestShowControl)
#include "test_show_control.moc"
