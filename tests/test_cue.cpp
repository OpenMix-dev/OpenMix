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
        QCOMPARE(cue.fadeTime(), 0.0);
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
        cue.setType(CueType::Fade);
        cue.setFadeTime(3.5);
        cue.setAutoFollow(true);
        cue.setAutoFollowDelay(1.0);

        QCOMPARE(cue.number(), 2.0);
        QCOMPARE(cue.name(), QString("My Cue"));
        QCOMPARE(cue.notes(), QString("Some notes"));
        QCOMPARE(cue.type(), CueType::Fade);
        QCOMPARE(cue.fadeTime(), 3.5);
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
        original.setType(CueType::Fade);
        original.setFadeTime(2.0);
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
        QCOMPARE(loaded.fadeTime(), original.fadeTime());
        QCOMPARE(loaded.autoFollow(), original.autoFollow());
        QCOMPARE(loaded.autoFollowDelay(), original.autoFollowDelay());
    }

    void testCueTypeConversion() {
        QCOMPARE(cueTypeToString(CueType::Snapshot), QString("snapshot"));
        QCOMPARE(cueTypeToString(CueType::Fade), QString("fade"));
        QCOMPARE(cueTypeToString(CueType::Stop), QString("stop"));
        QCOMPARE(cueTypeToString(CueType::GoTo), QString("goto"));
        QCOMPARE(cueTypeToString(CueType::Wait), QString("wait"));

        QCOMPARE(stringToCueType("snapshot"), CueType::Snapshot);
        QCOMPARE(stringToCueType("fade"), CueType::Fade);
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

    void testFadeCurve() {
        Cue cue;
        cue.setFadeCurve(FadeCurve::SCurve);

        QCOMPARE(cue.fadeCurve(), FadeCurve::SCurve);

        QJsonObject json = cue.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.fadeCurve(), FadeCurve::SCurve);
    }

    void testFadeCurveConversion() {
        QCOMPARE(fadeCurveToString(FadeCurve::Linear), QString("linear"));
        QCOMPARE(fadeCurveToString(FadeCurve::EaseIn), QString("easein"));
        QCOMPARE(fadeCurveToString(FadeCurve::EaseOut), QString("easeout"));
        QCOMPARE(fadeCurveToString(FadeCurve::SCurve), QString("scurve"));
        QCOMPARE(fadeCurveToString(FadeCurve::Exponential), QString("exponential"));

        QCOMPARE(stringToFadeCurve("linear"), FadeCurve::Linear);
        QCOMPARE(stringToFadeCurve("scurve"), FadeCurve::SCurve);
        QCOMPARE(stringToFadeCurve("invalid"), FadeCurve::Linear); // default
    }

    void testStopCue() {
        Cue cue;
        cue.setType(CueType::Stop);
        cue.setStopBehavior(StopBehavior::StopAndFadeOut);
        cue.setFadeTime(2.0);
        cue.setParameter("/dca/1/fader", 0.0);

        QCOMPARE(cue.type(), CueType::Stop);
        QCOMPARE(cue.stopBehavior(), StopBehavior::StopAndFadeOut);
        QCOMPARE(cue.fadeTime(), 2.0);
    }

    void testStopBehaviorSerialization() {
        Cue original;
        original.setType(CueType::Stop);
        original.setStopBehavior(StopBehavior::StopFadesOnly);

        QJsonObject json = original.toJson();
        Cue loaded = Cue::fromJson(json);

        QCOMPARE(loaded.stopBehavior(), StopBehavior::StopFadesOnly);
    }

    void testStopBehaviorConversion() {
        QCOMPARE(stopBehaviorToString(StopBehavior::StopFadesOnly), QString("stopfadesonly"));
        QCOMPARE(stopBehaviorToString(StopBehavior::StopAndApply), QString("stopandapply"));
        QCOMPARE(stopBehaviorToString(StopBehavior::StopAndFadeOut), QString("stopandfadeout"));

        QCOMPARE(stringToStopBehavior("stopfadesonly"), StopBehavior::StopFadesOnly);
        QCOMPARE(stringToStopBehavior("stopandfadeout"), StopBehavior::StopAndFadeOut);
        QCOMPARE(stringToStopBehavior("invalid"), StopBehavior::StopAndApply); // default
    }
};

QTEST_MAIN(TestCue)
#include "test_cue.moc"
