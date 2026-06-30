#include "core/TimecodeTrigger.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestTimecodeTrigger : public QObject {
    Q_OBJECT

  private slots:
    void firesWhenTimecodeReachesPoint() {
        TimecodeTriggerList registry;
        registry.addTrigger(0, 0, 10, 0, /*cue*/ 5.0);
        QSignalSpy spy(&registry, &TimecodeTriggerList::triggerFired);

        registry.onTimecode(0, 0, 9, 29); // before
        QCOMPARE(spy.count(), 0);

        registry.onTimecode(0, 0, 10, 0); // crosses the point
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), 5.0);
    }

    void firesOnlyOncePerCrossing() {
        TimecodeTriggerList registry;
        registry.addTrigger(0, 1, 0, 0, 12.0);
        QSignalSpy spy(&registry, &TimecodeTriggerList::triggerFired);

        registry.onTimecode(0, 0, 59, 29);
        registry.onTimecode(0, 1, 0, 0);
        registry.onTimecode(0, 1, 0, 1);
        registry.onTimecode(0, 1, 0, 2);
        QCOMPARE(spy.count(), 1); // armed once, fired once
    }

    void firesEvenIfExactFrameSkipped() {
        TimecodeTriggerList registry;
        registry.addTrigger(0, 0, 5, 10, 3.0);
        QSignalSpy spy(&registry, &TimecodeTriggerList::triggerFired);

        registry.onTimecode(0, 0, 5, 8);  // before
        registry.onTimecode(0, 0, 5, 14); // jumped past the exact frame -> still fires
        QCOMPARE(spy.count(), 1);
    }

    void reArmsAfterRewind() {
        TimecodeTriggerList registry;
        registry.addTrigger(0, 0, 10, 0, 5.0);
        QSignalSpy spy(&registry, &TimecodeTriggerList::triggerFired);

        registry.onTimecode(0, 0, 9, 0);
        registry.onTimecode(0, 0, 10, 0); // fire 1
        registry.onTimecode(0, 0, 5, 0);  // rewind before the point (re-arm)
        registry.onTimecode(0, 0, 10, 0); // fire 2
        QCOMPARE(spy.count(), 2);
    }

    void disabledTriggerDoesNotFire() {
        TimecodeTriggerList registry;
        const QString id = registry.addTrigger(0, 0, 10, 0, 5.0);
        registry.setTriggerEnabled(id, false);
        QSignalSpy spy(&registry, &TimecodeTriggerList::triggerFired);

        registry.onTimecode(0, 0, 9, 0);
        registry.onTimecode(0, 0, 10, 0);
        QCOMPARE(spy.count(), 0);
    }

    void serialization_roundTrip() {
        TimecodeTriggerList registry;
        registry.addTrigger(1, 2, 3, 4, 9.5);
        registry.addTrigger(0, 0, 30, 0, 2.0);

        TimecodeTriggerList restored;
        restored.loadFromJson(registry.toJson());
        QCOMPARE(restored.count(), 2);

        const TimecodeTrigger first = restored.triggers().at(0);
        QCOMPARE(first.hours, 1);
        QCOMPARE(first.minutes, 2);
        QCOMPARE(first.seconds, 3);
        QCOMPARE(first.frames, 4);
        QCOMPARE(first.cueNumber, 9.5);
    }
};

QTEST_MAIN(TestTimecodeTrigger)
#include "test_timecode_trigger.moc"
