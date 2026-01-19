#include "core/Cue.h"
#include <QtTest/QtTest>

using namespace StageBlend;

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
};

QTEST_MAIN(TestCue)
#include "test_cue.moc"
