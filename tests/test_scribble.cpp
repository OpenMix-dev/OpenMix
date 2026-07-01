#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/ChannelMonitor.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/ScribbleController.h"
#include "protocol/MixerProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

// Minimal mixer that records scribble pushes; everything else is a stub.
class RecordingMixer : public MixerProtocol {
  public:
    QList<QPair<int, QString>> nameCalls;
    QList<QPair<int, int>> colorCalls;
    bool linked = true;

    void setChannelName(int ch, const QString& name) override { nameCalls.append({ch, name}); }
    void setChannelColor(int ch, int color) override { colorCalls.append({ch, color}); }

    [[nodiscard]] QString protocolName() const override { return "mock"; }
    [[nodiscard]] QString protocolDescription() const override { return "mock"; }
    bool connect(const QString&, int) override { return true; }
    void disconnect() override {}
    [[nodiscard]] bool isConnected() const override { return linked; }
    [[nodiscard]] QString connectionStatus() const override { return {}; }
    [[nodiscard]] ConnectionState connectionState() const override {
        return ConnectionState::Connected;
    }
    void sendParameter(const QString&, const QVariant&) override {}
    [[nodiscard]] QVariant getParameter(const QString&) override { return {}; }
    void requestParameter(const QString&) override {}
    void requestParameterAsync(const QString&, ParameterCallback) override {}
    void recallSnapshot(const Cue&) override {}
    void recallScene(int) override {}
    void refresh() override {}
    [[nodiscard]] int latencyMs() const override { return 0; }

    [[nodiscard]] bool hasName(int ch, const QString& name) const {
        return nameCalls.contains({ch, name});
    }
};

class TestScribble : public QObject {
    Q_OBJECT

  private slots:
    void refreshNames_pushesActorNames() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Alice", 3));
        lib.addActor(Actor("Bob", 5));

        RecordingMixer mixer;
        ScribbleController ctl;
        ctl.setActorLibrary(&lib);
        ctl.setMixer(&mixer);

        mixer.nameCalls.clear();
        ctl.refreshNames();
        QCOMPARE(mixer.nameCalls.size(), 2);
        QVERIFY(mixer.hasName(3, "Alice"));
        QVERIFY(mixer.hasName(5, "Bob"));
    }

    void refreshNames_prefersLowestOrderActive() {
        ActorProfileLibrary lib;
        Actor lead("Lead", 4);
        lead.setOrder(1);
        Actor understudy("Understudy", 4);
        understudy.setOrder(2);
        lib.addActor(understudy);
        lib.addActor(lead);

        RecordingMixer mixer;
        ScribbleController ctl;
        ctl.setActorLibrary(&lib);
        ctl.setMixer(&mixer);
        mixer.nameCalls.clear();

        ctl.refreshNames();
        QCOMPARE(mixer.nameCalls.size(), 1);
        QVERIFY(mixer.hasName(4, "Lead"));
    }

    void setMixer_refreshesOnlyWhenConnected() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Cleo", 1));

        RecordingMixer offline;
        offline.linked = false;
        ScribbleController ctl;
        ctl.setActorLibrary(&lib);
        ctl.setMixer(&offline);
        QVERIFY(offline.nameCalls.isEmpty()); // not connected -> no push

        RecordingMixer online; // connected by default
        ctl.setMixer(&online);
        QVERIFY(online.hasName(1, "Cleo")); // pushed on attach
    }

    void channelState_mapsToColor() {
        RecordingMixer mixer;
        ScribbleController ctl;
        ctl.setMixer(&mixer);
        mixer.colorCalls.clear();

        ctl.onChannelStateChanged(7, static_cast<int>(ChannelState::Clipping));
        QCOMPARE(mixer.colorCalls.size(), 1);
        QCOMPARE(mixer.colorCalls[0].first, 7);
        QCOMPARE(mixer.colorCalls[0].second, ctl.stateColor(ChannelState::Clipping));

        ctl.onChannelStateChanged(7, static_cast<int>(ChannelState::Silent));
        QCOMPARE(mixer.colorCalls[1].second, ctl.stateColor(ChannelState::Silent));
    }

    void cueNumber_writtenToConfiguredChannel() {
        CueList cues;
        cues.addCue(Cue(5.0, "Top"));

        RecordingMixer mixer;
        ScribbleController ctl;
        ctl.setCueList(&cues);
        ctl.setMixer(&mixer);
        ctl.setCueNumberChannel(10);
        mixer.nameCalls.clear();

        ctl.onCurrentCueChanged(0);
        QCOMPARE(mixer.nameCalls.size(), 1);
        QCOMPARE(mixer.nameCalls[0].first, 10);
        QVERIFY(mixer.nameCalls[0].second.contains("5"));

        // disabled by default (channel 0) -> no cue push
        ScribbleController other;
        RecordingMixer m2;
        other.setCueList(&cues);
        other.setMixer(&m2);
        m2.nameCalls.clear();
        other.onCurrentCueChanged(0);
        QVERIFY(m2.nameCalls.isEmpty());
    }

    void cueChannel_notOverwrittenByActorName() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Alice", 3));
        lib.addActor(Actor("Bob", 10));

        RecordingMixer mixer;
        ScribbleController ctl;
        ctl.setActorLibrary(&lib);
        ctl.setCueNumberChannel(10); // reserved for cue number
        ctl.setMixer(&mixer);
        mixer.nameCalls.clear();

        ctl.refreshNames();
        QVERIFY(mixer.hasName(3, "Alice"));
        QVERIFY(!mixer.hasName(10, "Bob")); // channel 10 reserved
    }

    void disabled_suppressesAllPushes() {
        ActorProfileLibrary lib;
        lib.addActor(Actor("Alice", 3));

        RecordingMixer mixer;
        ScribbleController ctl;
        ctl.setActorLibrary(&lib);
        ctl.setMixer(&mixer);
        ctl.setEnabled(false);
        mixer.nameCalls.clear();
        mixer.colorCalls.clear();

        ctl.refreshNames();
        ctl.onChannelStateChanged(3, static_cast<int>(ChannelState::Clipping));
        QVERIFY(mixer.nameCalls.isEmpty());
        QVERIFY(mixer.colorCalls.isEmpty());
    }
};

QTEST_MAIN(TestScribble)
#include "test_scribble.moc"
