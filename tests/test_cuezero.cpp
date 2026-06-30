#include "core/CueZero.h"
#include "protocol/LoopbackProtocol.h"
#include "protocol/MixerCapabilities.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestCueZero : public QObject {
    Q_OBJECT

  private slots:
    void apply_pushesSceneLevelsAndLabels() {
        LoopbackProtocol mixer(MixerCapabilities::forConsole(ConsoleType::Loopback));
        mixer.connect("", 0); // loopback connects synchronously
        QVERIFY(mixer.isConnected());

        CueZero cz;
        cz.setBaseScene(5);
        cz.setLevel("/ch/01/mix/fader", 0.5);
        cz.setLevel("/ch/02/mix/fader", 0.25);
        cz.setLabel("/ch/01/config/name", "Lead Vox");

        QSignalSpy sceneSpy(&mixer, &MixerProtocol::sceneChanged);
        cz.apply(&mixer);

        // base scene recalled
        QCOMPARE(sceneSpy.count(), 1);
        QCOMPARE(sceneSpy.at(0).at(0).toInt(), 5);

        // levels + labels landed on the mixer (loopback records sends)
        QCOMPARE(mixer.getParameter("/ch/01/mix/fader").toDouble(), 0.5);
        QCOMPARE(mixer.getParameter("/ch/02/mix/fader").toDouble(), 0.25);
        QCOMPARE(mixer.getParameter("/ch/01/config/name").toString(), QString("Lead Vox"));
    }

    void apply_noSceneWhenBaseSceneUnset() {
        LoopbackProtocol mixer(MixerCapabilities::forConsole(ConsoleType::Loopback));
        mixer.connect("", 0);

        CueZero cz;
        cz.setLevel("/ch/03/mix/fader", 0.8);

        QSignalSpy sceneSpy(&mixer, &MixerProtocol::sceneChanged);
        cz.apply(&mixer);

        QCOMPARE(sceneSpy.count(), 0); // baseScene == -1, no recall
        QCOMPARE(mixer.getParameter("/ch/03/mix/fader").toDouble(), 0.8);
    }

    void safeValues_mergesLevelsAndLabels() {
        // Cue Zero doubles as PlaybackGuard safe-values (what PANIC restores to)
        CueZero cz;
        cz.setLevel("/ch/01/mix/fader", 0.0);
        cz.setLabel("/ch/01/config/name", "Reset");

        const QJsonObject safe = cz.safeValues();
        QCOMPARE(safe.value("/ch/01/mix/fader").toDouble(), 0.0);
        QCOMPARE(safe.value("/ch/01/config/name").toString(), QString("Reset"));
        QCOMPARE(safe.size(), 2);
    }

    void serialization_roundTrip() {
        CueZero cz;
        cz.setBaseScene(7);
        cz.setLevel("/main/st/mix/fader", 0.75);
        cz.setLabel("/dca/1/config/name", "Band");

        CueZero restored;
        restored.loadFromJson(cz.toJson());

        QCOMPARE(restored.baseScene(), 7);
        QCOMPARE(restored.levels().value("/main/st/mix/fader").toDouble(), 0.75);
        QCOMPARE(restored.labels().value("/dca/1/config/name").toString(), QString("Band"));
    }

    void clear_resetsEverything() {
        CueZero cz;
        cz.setBaseScene(3);
        cz.setLevel("/x", 1.0);
        QVERIFY(!cz.isEmpty());
        cz.clear();
        QVERIFY(cz.isEmpty());
        QCOMPARE(cz.baseScene(), -1);
    }
};

QTEST_MAIN(TestCueZero)
#include "test_cuezero.moc"
