#include "core/Cue.h"
#include "core/Position.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestPosition : public QObject {
    Q_OBJECT

  private slots:
    void position_roundTrip() {
        Position p("Downstage Left", "DSL");
        p.setPan(-0.75);
        p.setDelay(12.5);
        p.setBuses({1, 3, 5});

        const QJsonObject json = p.toJson();
        const Position restored = Position::fromJson(json);

        QCOMPARE(restored.id(), p.id());
        QCOMPARE(restored.name(), QString("Downstage Left"));
        QCOMPARE(restored.shortName(), QString("DSL"));
        QCOMPARE(restored.pan(), -0.75);
        QCOMPARE(restored.delay(), 12.5);
        QCOMPARE(restored.buses(), QList<int>({1, 3, 5}));
    }

    void position_defaultsAndEmptyBuses() {
        Position p("Center");
        const Position restored = Position::fromJson(p.toJson());
        QCOMPARE(restored.pan(), 0.0);
        QCOMPARE(restored.delay(), 0.0);
        QVERIFY(restored.buses().isEmpty());
    }

    void library_addEmitsSignals() {
        PositionLibrary lib;
        QSignalSpy addSpy(&lib, &PositionLibrary::positionAdded);
        QSignalSpy changedSpy(&lib, &PositionLibrary::changed);

        Position a("A");
        const QString idA = lib.addPosition(a);

        QCOMPARE(lib.count(), 1);
        QCOMPARE(addSpy.count(), 1);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(addSpy.at(0).at(0).toString(), idA);
        QVERIFY(lib.contains(idA));
    }

    void library_roundTrip() {
        PositionLibrary lib;
        Position a("A");
        Position b("B");
        b.setPan(0.5);
        b.setDelay(20.0);
        b.setBuses({2, 4});
        const QString idA = lib.addPosition(a);
        const QString idB = lib.addPosition(b);
        QCOMPARE(lib.count(), 2);

        PositionLibrary restored;
        restored.loadFromJson(lib.toJson());

        QCOMPARE(restored.count(), 2);
        const auto pa = restored.position(idA);
        const auto pb = restored.position(idB);
        QVERIFY(pa.has_value());
        QVERIFY(pb.has_value());
        QCOMPARE(pa->name(), QString("A"));
        QCOMPARE(pb->pan(), 0.5);
        QCOMPARE(pb->delay(), 20.0);
        QCOMPARE(pb->buses(), QList<int>({2, 4}));
    }

    void library_updateAndRemove() {
        PositionLibrary lib;
        Position a("A");
        const QString idA = lib.addPosition(a);

        Position edited = lib.position(idA).value();
        edited.setName("A-edited");
        edited.setPan(-0.25);
        QSignalSpy changedSpy(&lib, &PositionLibrary::positionChanged);
        lib.updatePosition(edited);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(lib.position(idA)->name(), QString("A-edited"));
        QCOMPARE(lib.position(idA)->pan(), -0.25);

        QSignalSpy removeSpy(&lib, &PositionLibrary::positionRemoved);
        QVERIFY(lib.removePosition(idA));
        QCOMPARE(removeSpy.count(), 1);
        QVERIFY(lib.isEmpty());
        QVERIFY(!lib.removePosition(idA)); // already gone
    }

    void cue_channelPositions_roundTrip() {
        Cue cue(1.0, "Scene");
        cue.setChannelPosition(1, "pos-a");
        cue.setChannelPosition(2, "pos-b");
        cue.setChannelPosition(3, "");  // empty clears
        QCOMPARE(cue.channelPositions().size(), 2);

        const Cue restored = Cue::fromJson(cue.toJson());
        QCOMPARE(restored.channelPositions().size(), 2);
        QCOMPARE(restored.channelPosition(1), QString("pos-a"));
        QCOMPARE(restored.channelPosition(2), QString("pos-b"));
        QVERIFY(restored.channelPosition(3).isEmpty());
    }
};

QTEST_MAIN(TestPosition)
#include "test_position.moc"
