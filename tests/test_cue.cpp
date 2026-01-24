#include "core/Cue.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestCue : public QObject {
    Q_OBJECT

  private slots:
    void testDefaultConstructor() {
        Cue cue;
        QVERIFY(!cue.id().isEmpty());
        QCOMPARE(cue.number(), 0.0);
        QVERIFY(cue.name().isEmpty());
        QCOMPARE(cue.type(), CueType::Snapshot);
        QCOMPARE(cue.autoFollow(), false);
    }

    void testConstructorWithArgs() {
        Cue cue(1.5, "Test Cue");
        QCOMPARE(cue.number(), 1.5);
        QCOMPARE(cue.name(), QString("Test Cue"));
    }

    void testSetters() {
        Cue cue;
        cue.setNumber(2.0);
        cue.setName("My Cue");
        cue.setNotes("Some notes");
        cue.setType(CueType::Snapshot);
        cue.setAutoFollow(true);
        cue.setAutoFollowDelay(1.0);

        QCOMPARE(cue.number(), 2.0);
        QCOMPARE(cue.name(), QString("My Cue"));
        QCOMPARE(cue.notes(), QString("Some notes"));
        QCOMPARE(cue.type(), CueType::Snapshot);
        QCOMPARE(cue.autoFollow(), true);
        QCOMPARE(cue.autoFollowDelay(), 1.0);
    }

    void testParameters() {
        Cue cue;
        cue.setParameter("/ch/01/mix/fader", 0.75);
        cue.setParameter("/ch/01/mix/on", 1);

        QCOMPARE(cue.parameter("/ch/01/mix/fader").toDouble(), 0.75);
        QCOMPARE(cue.parameter("/ch/01/mix/on").toInt(), 1);
        QVERIFY(!cue.parameter("/nonexistent").isValid());
    }

    void testJsonSerialization() {
        Cue original(1.0, "Test");
        original.setNotes("Test notes");
        original.setType(CueType::Snapshot);
        original.setAutoFollow(true);
        original.setAutoFollowDelay(0.5);
        original.setParameter("/test", 0.5);

        QJsonObject json = original.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.id(), original.id());
        QCOMPARE(loaded.number(), original.number());
        QCOMPARE(loaded.name(), original.name());
        QCOMPARE(loaded.notes(), original.notes());
        QCOMPARE(loaded.type(), original.type());
        QCOMPARE(loaded.autoFollow(), original.autoFollow());
        QCOMPARE(loaded.autoFollowDelay(), original.autoFollowDelay());
    }

    void testCueTypeConversion() {
        QCOMPARE(cueTypeToString(CueType::Snapshot), QString("snapshot"));
        QCOMPARE(cueTypeToString(CueType::Stop), QString("stop"));
        QCOMPARE(cueTypeToString(CueType::GoTo), QString("goto"));
        QCOMPARE(cueTypeToString(CueType::Wait), QString("wait"));

        QCOMPARE(stringToCueType("snapshot"), CueType::Snapshot);
        QCOMPARE(stringToCueType("stop"), CueType::Stop);
        QCOMPARE(stringToCueType("invalid"), CueType::Snapshot); // default
    }

    void testComparison() {
        Cue cue1(1.0);
        Cue cue2(2.0);
        QVERIFY(cue1 < cue2);
        QVERIFY(!(cue2 < cue1));
    }

    void testGotoTarget() {
        Cue cue;
        cue.setType(CueType::GoTo);
        cue.setGotoTarget("5.0");
        cue.setGotoAutoExecute(true);

        QCOMPARE(cue.type(), CueType::GoTo);
        QCOMPARE(cue.gotoTarget(), QString("5.0"));
        QCOMPARE(cue.gotoAutoExecute(), true);
    }

    void testGotoTargetSerialization() {
        Cue original;
        original.setType(CueType::GoTo);
        original.setGotoTarget("10.5");
        original.setGotoAutoExecute(true);

        QJsonObject json = original.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.type(), CueType::GoTo);
        QCOMPARE(loaded.gotoTarget(), QString("10.5"));
        QCOMPARE(loaded.gotoAutoExecute(), true);
    }

    void testGotoTargetById() {
        Cue cue;
        cue.setType(CueType::GoTo);
        cue.setGotoTarget("abc-123-def"); // cue ID instead of number
        cue.setGotoAutoExecute(false);

        QJsonObject json = cue.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.gotoTarget(), QString("abc-123-def"));
        QCOMPARE(loaded.gotoAutoExecute(), false);
    }

    void testMacroCue() {
        Cue cue;
        cue.setType(CueType::Macro);
        cue.setMacroExecutionMode(MacroExecutionMode::Sequential);
        cue.addChildCueId("child-1");
        cue.addChildCueId("child-2");

        QCOMPARE(cue.childCueIds().size(), 2);
        QCOMPARE(cue.macroExecutionMode(), MacroExecutionMode::Sequential);

        QJsonObject json = cue.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.childCueIds().size(), 2);
        QCOMPARE(loaded.childCueIds().at(0), QString("child-1"));
        QCOMPARE(loaded.macroExecutionMode(), MacroExecutionMode::Sequential);
    }

    void testMacroParallelMode() {
        Cue cue;
        cue.setType(CueType::Macro);
        cue.setMacroExecutionMode(MacroExecutionMode::Parallel);

        QJsonObject json = cue.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.macroExecutionMode(), MacroExecutionMode::Parallel);
    }

    void testDCATargeting() {
        Cue cue;

        QVERIFY(cue.targetsAllDCAs());

        cue.addTargetedDCA(1);
        cue.addTargetedDCA(3);

        QVERIFY(!cue.targetsAllDCAs());
        QVERIFY(cue.targetsDCA(1));
        QVERIFY(!cue.targetsDCA(2));
        QVERIFY(cue.targetsDCA(3));

        QJsonObject json = cue.toJson();
        Cue loaded = Cue::fromJson(json);

        QVERIFY(!loaded.targetsAllDCAs());
        QVERIFY(loaded.targetsDCA(1));
        QVERIFY(loaded.targetsDCA(3));
    }

    void testDCAOverrides() {
        Cue cue;

        DCAOverride override;
        override.mute = true;
        override.label = "Vocals";

        cue.setDCAOverride(1, override);

        QVERIFY(cue.dcaOverrides().contains(1));
        QCOMPARE(cue.dcaOverrides()[1].mute.value(), true);
        QCOMPARE(cue.dcaOverrides()[1].label.value(), QString("Vocals"));

        QJsonObject json = cue.toJson();
        Cue loaded = Cue::fromJson(json);

        QVERIFY(loaded.dcaOverrides().contains(1));
        QCOMPARE(loaded.dcaOverrides()[1].mute.value(), true);
        QCOMPARE(loaded.dcaOverrides()[1].label.value(), QString("Vocals"));
    }

    void testStopCue() {
        Cue cue;
        cue.setType(CueType::Stop);
        cue.setStopBehavior(StopBehavior::StopAndApply);
        cue.setParameter("/dca/1/mute", 1);

        QCOMPARE(cue.type(), CueType::Stop);
        QCOMPARE(cue.stopBehavior(), StopBehavior::StopAndApply);
    }

    void testStopBehaviorSerialization() {
        Cue original;
        original.setType(CueType::Stop);
        original.setStopBehavior(StopBehavior::StopOnly);

        QJsonObject json = original.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.stopBehavior(), StopBehavior::StopOnly);
    }

    void testStopBehaviorConversion() {
        QCOMPARE(stopBehaviorToString(StopBehavior::StopOnly), QString("stoponly"));
        QCOMPARE(stopBehaviorToString(StopBehavior::StopAndApply), QString("stopandapply"));

        QCOMPARE(stringToStopBehavior("stoponly"), StopBehavior::StopOnly);
        QCOMPARE(stringToStopBehavior("stopandapply"), StopBehavior::StopAndApply);
        QCOMPARE(stringToStopBehavior("invalid"), StopBehavior::StopAndApply); // default
    }
};

QTEST_MAIN(TestCue)
#include "test_cue.moc"
