#include "core/Cue.h"
#include "core/DCAMapping.h"
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

    void testMergeContentPreservesIdentity() {
        Cue target(5.0, "Target");
        target.setChannelProfile(1, "lead");
        Cue source(9.0, "Source");
        source.setNotes("src notes");
        source.setChannelProfile(2, "backup");
        source.setChannelLevel(3, 0.5);
        source.setColor("#ff0000");

        const QString keepId = target.id();
        target.mergeContentFrom(source);

        // identity preserved
        QCOMPARE(target.id(), keepId);
        QCOMPARE(target.number(), 5.0);
        QCOMPARE(target.name(), QString("Target"));
        QCOMPARE(target.notes(), QString()); // notes are identity, not merged

        // content united, other winning
        QCOMPARE(target.channelProfiles().value(1), QString("lead"));
        QCOMPARE(target.channelProfiles().value(2), QString("backup"));
        QCOMPARE(target.channelLevels().value(3), 0.5);
        QCOMPARE(target.color(), QString("#ff0000"));
    }

    void testSwapContentKeepsIdentity() {
        Cue a(1.0, "A");
        a.setColor("#111111");
        a.setChannelProfile(1, "aProfile");
        Cue b(2.0, "B");
        b.setColor("#222222");
        b.setChannelProfile(2, "bProfile");

        const QString aId = a.id();
        const QString bId = b.id();
        a.swapContentWith(b);

        QCOMPARE(a.id(), aId);
        QCOMPARE(a.number(), 1.0);
        QCOMPARE(a.name(), QString("A"));
        QCOMPARE(b.id(), bId);
        QCOMPARE(b.number(), 2.0);

        // content exchanged
        QCOMPARE(a.color(), QString("#222222"));
        QCOMPARE(a.channelProfiles().value(2), QString("bProfile"));
        QVERIFY(!a.channelProfiles().contains(1));
        QCOMPARE(b.color(), QString("#111111"));
        QCOMPARE(b.channelProfiles().value(1), QString("aProfile"));
    }

    void testAssignChannelToDCAMapping_seedsFromShowMapping() {
        DCAMapping show;
        show.assignChannelToDCA(1, 1);
        show.assignChannelToDCA(2, 2);

        Cue cue(1.0, "Opening");
        QVERIFY(!cue.hasCustomDCAMapping());

        cue.assignChannelToDCAMapping(2, 1, &show);

        QVERIFY(cue.hasCustomDCAMapping());
        QCOMPARE(cue.dcaChannelMapping().value(1), QList<int>({1, 2}));
        QVERIFY(cue.dcaChannelMapping().value(2).isEmpty());
        // show mapping untouched
        QCOMPARE(show.channelsForDCA(2), QList<int>({2}));
    }

    void testAssignChannelToDCAMapping_movesWithinExistingCustom() {
        Cue cue;
        QMap<int, QList<int>> mapping;
        mapping[1] = {5};
        cue.setDCAChannelMapping(mapping);

        cue.assignChannelToDCAMapping(5, 3, nullptr);

        QVERIFY(cue.dcaChannelMapping().value(1).isEmpty());
        QCOMPARE(cue.dcaChannelMapping().value(3), QList<int>({5}));
    }

    void testAssignChannelToDCAMapping_nullSeedStartsEmpty() {
        Cue cue;
        cue.assignChannelToDCAMapping(7, 4, nullptr);

        QVERIFY(cue.hasCustomDCAMapping());
        QCOMPARE(cue.dcaChannelMapping().value(4), QList<int>({7}));

        // round-trips through JSON
        Cue r = Cue::fromJson(cue.toJson());
        QCOMPARE(r.dcaChannelMapping().value(4), QList<int>({7}));
    }

    void testAssignChannelsToDCAMapping_seedsAndReplacesTargetDca() {
        DCAMapping show;
        show.assignChannelToDCA(1, 1);
        show.assignChannelToDCA(2, 1);
        show.assignChannelToDCA(9, 2);

        Cue cue(1.0, "Opening");
        cue.assignChannelsToDCAMapping({5, 6}, 1, &show);

        QVERIFY(cue.hasCustomDCAMapping());
        // the label declares DCA 1's membership: seeded {1,2} replaced by {5,6}
        QCOMPARE(cue.dcaChannelMapping().value(1), QList<int>({5, 6}));
        QCOMPARE(cue.dcaChannelMapping().value(2), QList<int>({9}));
        // show mapping untouched
        QCOMPARE(show.channelsForDCA(1), QList<int>({1, 2}));
    }

    void testAssignChannelsToDCAMapping_movesChannelsOffOtherDCAs() {
        Cue cue;
        QMap<int, QList<int>> mapping;
        mapping[1] = {5};
        mapping[2] = {6, 7};
        cue.setDCAChannelMapping(mapping);

        cue.assignChannelsToDCAMapping({5, 6}, 3, nullptr);

        QVERIFY(cue.dcaChannelMapping().value(1).isEmpty());
        QCOMPARE(cue.dcaChannelMapping().value(2), QList<int>({7}));
        QCOMPARE(cue.dcaChannelMapping().value(3), QList<int>({5, 6}));
    }

    void testAssignChannelsToDCAMapping_retypeSwaps() {
        Cue cue;
        cue.assignChannelsToDCAMapping({5, 6}, 1, nullptr);
        cue.assignChannelsToDCAMapping({8}, 1, nullptr);

        QCOMPARE(cue.dcaChannelMapping().value(1), QList<int>({8}));
        for (const QList<int>& channels : cue.dcaChannelMapping()) {
            QVERIFY(!channels.contains(5));
            QVERIFY(!channels.contains(6));
        }

        // multi-channel list round-trips through JSON
        cue.assignChannelsToDCAMapping({3, 4}, 2, nullptr);
        Cue r = Cue::fromJson(cue.toJson());
        QCOMPARE(r.dcaChannelMapping().value(2), QList<int>({3, 4}));
    }
};

QTEST_MAIN(TestCue)
#include "test_cue.moc"
