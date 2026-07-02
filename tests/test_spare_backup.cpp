#include "core/SpareBackup.h"
#include <QJsonObject>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestSpareBackup : public QObject {
    Q_OBJECT

  private slots:
    void testAllocateRequiresSpare() {
        SpareBackup sb;
        QVERIFY(!sb.allocateTo(5)); // no spare configured yet
        sb.setSpareChannel(40);
        QVERIFY(sb.allocateTo(5));
        QCOMPARE(sb.allocatedChannel(), 5);
        QVERIFY(sb.isAllocated());
        QCOMPARE(sb.state(), SpareBackup::State::Inactive);
    }

    void testSwitchNowBlocksReallocation() {
        SpareBackup sb;
        sb.setSpareChannel(40);
        sb.allocateTo(5);
        sb.switchNow();
        QVERIFY(sb.isActive());
        QVERIFY(!sb.allocateTo(6)); // live spare can't be reallocated
        QCOMPARE(sb.allocatedChannel(), 5);
        sb.revert();
        QCOMPARE(sb.state(), SpareBackup::State::Inactive);
        QVERIFY(sb.allocateTo(6));
    }

    void testSwitchLaterPromotesOnCue() {
        SpareBackup sb;
        sb.setSpareChannel(40);
        sb.allocateTo(5);
        sb.switchLater();
        QCOMPARE(sb.state(), SpareBackup::State::Armed);
        sb.onCueFired();
        QCOMPARE(sb.state(), SpareBackup::State::Active);
    }

    void testDroppingSpareClears() {
        SpareBackup sb;
        sb.setSpareChannel(40);
        sb.allocateTo(5);
        sb.switchNow();
        sb.setSpareChannel(-1);
        QVERIFY(!sb.isAllocated());
        QCOMPARE(sb.state(), SpareBackup::State::Inactive);
    }

    void testRoundTrip() {
        SpareBackup sb;
        sb.setSpareChannel(40);
        sb.allocateTo(5);
        sb.switchLater();

        SpareBackup restored;
        restored.loadFromJson(sb.toJson());
        QCOMPARE(restored.spareChannel(), 40);
        QCOMPARE(restored.allocatedChannel(), 5);
        QCOMPARE(restored.state(), SpareBackup::State::Armed);
    }
};

QTEST_MAIN(TestSpareBackup)
#include "test_spare_backup.moc"
