#include "core/Cue.h"
#include "core/CueList.h"
#include "core/DCAMapping.h"
#include "core/PlaybackEngine.h"
#include "protocol/LoopbackProtocol.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {
constexpr quint32 dcaBit(int d) { return 1u << (d - 1); }

LoopbackProtocol* makeConnectedLoopback(QObject* parent) {
    auto* mixer = new LoopbackProtocol(MixerCapabilities::forConsole(ConsoleType::X32), parent);
    [[maybe_unused]] const bool ok = mixer->connect("loopback", 0);
    Q_ASSERT(ok);
    return mixer;
}

// overrides no DCA setters, so it exercises MixerProtocol's generic-path defaults
class RecordingProtocol : public MixerProtocol {
  public:
    [[nodiscard]] QString protocolName() const override { return "recording"; }
    [[nodiscard]] QString protocolDescription() const override { return "records sends"; }
    [[nodiscard]] bool connect(const QString&, int) override { return true; }
    void disconnect() override {}
    [[nodiscard]] bool isConnected() const override { return true; }
    [[nodiscard]] QString connectionStatus() const override { return "connected"; }
    [[nodiscard]] ConnectionState connectionState() const override {
        return ConnectionState::Connected;
    }
    void sendParameter(const QString& path, const QVariant& value) override {
        sends.append(qMakePair(path, value));
    }
    [[nodiscard]] QVariant getParameter(const QString&) override { return {}; }
    void requestParameter(const QString&) override {}
    void requestParameterAsync(const QString&, ParameterCallback) override {}
    void recallSnapshot(const Cue&) override {}
    void recallScene(int) override {}
    void refresh() override {}
    [[nodiscard]] int latencyMs() const override { return 0; }

    QList<QPair<QString, QVariant>> sends;
};
} // namespace

class TestPlaybackDcaAssign : public QObject {
    Q_OBJECT

  private:
    static quint32 chMask(LoopbackProtocol* m, int ch) {
        auto v = m->readChannelDcaMask(ch);
        return v.value_or(0u);
    }
    static quint32 busMask(LoopbackProtocol* m, int bus) {
        auto v = m->readBusDcaMask(bus);
        return v.value_or(0u);
    }

  private slots:
    void showMapping_addsRemovesPreserves() {
        DCAMapping mapping;
        mapping.assignChannelToDCA(1, 3); // (channel, dca): ch1 -> DCA3
        mapping.assignChannelToDCA(2, 3);
        mapping.assignBusToDCA(1, 3);

        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cues.addCue(cue);

        auto* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setDCAMapping(&mapping);
        engine.setMixer(mixer);

        // loopback seeds ch1/ch2 in DCA1, ch9 in DCA3, bus5 in DCA3
        QCOMPARE(chMask(mixer, 1), dcaBit(1));
        QCOMPARE(chMask(mixer, 9), dcaBit(3));
        QCOMPARE(busMask(mixer, 5), dcaBit(3));

        engine.executeCue(0);

        QCOMPARE(chMask(mixer, 1), dcaBit(1) | dcaBit(3));
        QCOMPARE(chMask(mixer, 2), dcaBit(1) | dcaBit(3));
        QCOMPARE(chMask(mixer, 9), 0u);
        QCOMPARE(busMask(mixer, 1), dcaBit(1) | dcaBit(3));
        QCOMPARE(busMask(mixer, 5), 0u);
    }

    void customMapping_overridesThenShowRestores() {
        DCAMapping mapping;
        mapping.assignChannelToDCA(5, 1); // show: DCA1 -> ch5

        CueList cues;
        Cue custom(1.0, "custom");
        custom.setType(CueType::Snapshot);
        custom.setDCAChannelMapping({{1, {6}}}); // cue: DCA1 -> ch6
        cues.addCue(custom);
        Cue plain(2.0, "plain");
        plain.setType(CueType::Snapshot);
        cues.addCue(plain);

        auto* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setDCAMapping(&mapping);
        engine.setMixer(mixer);

        engine.executeCue(0); // custom cue
        QVERIFY((chMask(mixer, 6) & dcaBit(1)) != 0);
        QCOMPARE(chMask(mixer, 5) & dcaBit(1), 0u);

        engine.executeCue(1); // plain cue restores show mapping
        QVERIFY((chMask(mixer, 5) & dcaBit(1)) != 0);
        QCOMPARE(chMask(mixer, 6) & dcaBit(1), 0u);
    }

    void inactiveDca_membershipPreserved() {
        DCAMapping mapping;
        mapping.assignChannelToDCA(7, 2); // DCA2 (marked inactive below) -> ch7
        mapping.assignChannelToDCA(8, 3); // DCA3 -> ch8

        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cues.addCue(cue);

        auto* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setDCAMapping(&mapping);
        engine.setInactiveDcas({2});
        engine.setMixer(mixer);

        QCOMPARE(chMask(mixer, 7), dcaBit(2));
        QCOMPARE(chMask(mixer, 8), dcaBit(2));

        engine.executeCue(0);

        QCOMPARE(chMask(mixer, 7), dcaBit(2));
        QCOMPARE(chMask(mixer, 8), dcaBit(2) | dcaBit(3));
    }

    void targetedSubset_leavesOtherDcasAlone() {
        DCAMapping mapping;
        mapping.assignChannelToDCA(3, 1); // DCA1 -> ch3
        mapping.assignChannelToDCA(3, 4); // DCA4 -> ch3

        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cue.setTargetedDCAs({4});
        cues.addCue(cue);

        auto* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setDCAMapping(&mapping);
        engine.setMixer(mixer);

        QCOMPARE(chMask(mixer, 3), dcaBit(1));

        engine.executeCue(0);

        QCOMPARE(chMask(mixer, 3), dcaBit(1) | dcaBit(4));
    }

    void muteUnassign_clearsMembers() {
        DCAMapping mapping;
        mapping.assignChannelToDCA(8, 5); // DCA5 -> ch8

        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        DCAOverride ov;
        ov.mute = true;
        cue.setDCAOverride(5, ov);
        cues.addCue(cue);

        auto* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setDCAMapping(&mapping);
        engine.setMuteDcaUnassign(true);
        engine.setMixer(mixer);
        mixer->clearRecordedCalls();

        engine.executeCue(0);

        QCOMPARE(chMask(mixer, 8) & dcaBit(5), 0u);
        QVERIFY((chMask(mixer, 8) & dcaBit(2)) != 0);
        QVERIFY(mixer->recordedCalls().contains("dcamute:dca=5:muted=1"));
    }

    void noMapping_writesNothing() {
        CueList cues;
        Cue cue(1.0, "A");
        cue.setType(CueType::Snapshot);
        cues.addCue(cue);

        auto* mixer = makeConnectedLoopback(this);
        PlaybackEngine engine;
        engine.setCueList(&cues);
        engine.setMixer(mixer);
        mixer->clearRecordedCalls();

        engine.executeCue(0);

        for (const QString& call : mixer->recordedCalls())
            QVERIFY2(!call.startsWith("dcamask:"), qPrintable(call));
    }

    void baseDefaults_useGenericPaths() {
        RecordingProtocol proto;
        proto.setDcaMute(3, true);
        proto.setDcaFaderDb(2, 0.5);
        proto.setDcaName(1, "Band");
        proto.setChannelDcaMask(5, 7); // default no-op

        QCOMPARE(proto.sends.size(), 3);
        QCOMPARE(proto.sends[0].first, QString("/dca/3/mute"));
        QCOMPARE(proto.sends[0].second.toInt(), 1);
        QCOMPARE(proto.sends[1].first, QString("/dca/2/fader"));
        QCOMPARE(proto.sends[2].first, QString("/dca/1/config/name"));
        QCOMPARE(proto.sends[2].second.toString(), QString("Band"));

        QVERIFY(!proto.readChannelDcaMask(5).has_value());
        QVERIFY(!proto.readBusDcaMask(5).has_value());
    }
};

QTEST_MAIN(TestPlaybackDcaAssign)
#include "test_playback_dca_assign.moc"
