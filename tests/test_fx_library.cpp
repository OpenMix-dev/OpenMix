#include "core/FxLibrary.h"
#include <QList>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestFxLibrary : public QObject {
    Q_OBJECT

  private slots:
    void testNamedUnits() {
        FxLibrary fx;
        fx.setUnit(1, "Reverb");
        fx.setUnit(2, "Delay");
        QCOMPARE(fx.unitName(1), QString("Reverb"));
        QCOMPARE(fx.units().size(), 2);
        QCOMPARE(fx.units().first().index, 1); // sorted by index
        fx.setUnit(1, "Hall");                 // rename, not add
        QCOMPARE(fx.unitName(1), QString("Hall"));
        QCOMPARE(fx.units().size(), 2);
    }

    void testChannelAssignment() {
        FxLibrary fx;
        fx.setUnit(1, "Reverb");
        fx.setUnit(2, "Delay");
        fx.setChannelAssignment(5, {1, 2});
        QCOMPARE(fx.channelAssignment(5), QList<int>({1, 2}));
        fx.setChannelAssignment(5, {}); // empty clears
        QVERIFY(fx.channelAssignment(5).isEmpty());
    }

    void testRemoveUnitDropsReferences() {
        FxLibrary fx;
        fx.setUnit(1, "Reverb");
        fx.setUnit(2, "Delay");
        fx.setChannelAssignment(5, {1, 2});
        fx.setDefaultFx(1);
        fx.removeUnit(1);
        QVERIFY(!fx.hasUnit(1));
        QCOMPARE(fx.channelAssignment(5), QList<int>({2}));
        QCOMPARE(fx.defaultFx(), -1);
    }

    void testRoundTrip() {
        FxLibrary fx;
        fx.setUnit(1, "Reverb");
        fx.setChannelAssignment(5, {1});
        fx.setDefaultFx(1);

        FxLibrary restored;
        restored.loadFromJson(fx.toJson());
        QCOMPARE(restored.unitName(1), QString("Reverb"));
        QCOMPARE(restored.channelAssignment(5), QList<int>({1}));
        QCOMPARE(restored.defaultFx(), 1);
    }
};

QTEST_MAIN(TestFxLibrary)
#include "test_fx_library.moc"
