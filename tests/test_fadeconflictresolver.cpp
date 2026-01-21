#include "core/Cue.h"
#include "core/CueList.h"
#include "core/FadeConflictResolver.h"
#include "core/PlaybackEngine.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestFadeConflictResolver : public QObject {
    Q_OBJECT

  private slots:
    void testInitialState() {
        FadeConflictResolver resolver;
        QCOMPARE(resolver.resolutionPolicy(), ConflictResolution::LastWins);
        QVERIFY(resolver.liveEditOverrides().isEmpty());
    }

    void testSetResolutionPolicy() {
        FadeConflictResolver resolver;

        resolver.setResolutionPolicy(ConflictResolution::FirstWins);
        QCOMPARE(resolver.resolutionPolicy(), ConflictResolution::FirstWins);

        resolver.setResolutionPolicy(ConflictResolution::LiveEditOverride);
        QCOMPARE(resolver.resolutionPolicy(), ConflictResolution::LiveEditOverride);
    }

    void testSetLiveEditOverrides() {
        FadeConflictResolver resolver;

        QStringList overrides;
        overrides << "/dca/1/fader" << "/dca/2/fader";

        resolver.setLiveEditOverrides(overrides);
        QCOMPARE(resolver.liveEditOverrides().size(), 2);
        QVERIFY(resolver.isLiveEditOverride("/dca/1/fader"));
        QVERIFY(resolver.isLiveEditOverride("/dca/2/fader"));
        QVERIFY(!resolver.isLiveEditOverride("/dca/3/fader"));
    }

    void testClearLiveEditOverrides() {
        FadeConflictResolver resolver;

        QStringList overrides;
        overrides << "/dca/1/fader";
        resolver.setLiveEditOverrides(overrides);

        QVERIFY(!resolver.liveEditOverrides().isEmpty());

        resolver.clearLiveEditOverrides();
        QVERIFY(resolver.liveEditOverrides().isEmpty());
        QVERIFY(!resolver.isLiveEditOverride("/dca/1/fader"));
    }

    void testIsLiveEditOverride() {
        FadeConflictResolver resolver;

        QVERIFY(!resolver.isLiveEditOverride("/dca/1/fader"));

        QStringList overrides;
        overrides << "/dca/1/fader";
        resolver.setLiveEditOverrides(overrides);

        QVERIFY(resolver.isLiveEditOverride("/dca/1/fader"));
        QVERIFY(!resolver.isLiveEditOverride("/dca/2/fader"));
    }

    void testResolveConflictsWithNoFades() {
        FadeConflictResolver resolver;

        QVector<FadeInstance> emptyFades;
        QJsonObject result = resolver.resolveConflicts(emptyFades);

        QVERIFY(result.isEmpty());
    }

    void testResolveConflictsSkipsLiveEditOverrides() {
        FadeConflictResolver resolver;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cue.setParameter("/dca/2/fader", 0.6);
        cueList.addCue(cue);

        resolver.setCueList(&cueList);

        // mark /dca/1/fader as live-edited
        QStringList overrides;
        overrides << "/dca/1/fader";
        resolver.setLiveEditOverrides(overrides);

        // create fade instance
        QJsonObject startParams;
        startParams["/dca/1/fader"] = 0.0;
        startParams["/dca/2/fader"] = 0.0;

        QJsonObject endParams;
        endParams["/dca/1/fader"] = 1.0;
        endParams["/dca/2/fader"] = 1.0;

        FadeInstance fade(cue.id(), startParams, endParams, 2.0, FadeCurve::Linear);

        QVector<FadeInstance> activeFades;
        activeFades.append(fade);

        QJsonObject result = resolver.resolveConflicts(activeFades);

        // /dca/1/fader should be excluded (live edit override)
        QVERIFY(!result.contains("/dca/1/fader"));

        // /dca/2/fader should be included
        QVERIFY(result.contains("/dca/2/fader"));
    }
};

QTEST_MAIN(TestFadeConflictResolver)
#include "test_fadeconflictresolver.moc"
