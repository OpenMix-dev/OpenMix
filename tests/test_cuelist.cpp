#include "core/CueList.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestCueList : public QObject {
    Q_OBJECT

  private slots:
    void testEmpty() {
        CueList list;
        QCOMPARE(list.count(), 0);
        QVERIFY(list.isEmpty());
    }

    void testAddCue() {
        CueList list;
        QSignalSpy spy(&list, &CueList::cueAdded);

        Cue cue(1.0, "First");
        list.addCue(cue);

        QCOMPARE(list.count(), 1);
        QCOMPARE(list.at(0).name(), QString("First"));
        QCOMPARE(spy.count(), 1);
    }

    void testInsertCue() {
        CueList list;
        list.addCue(Cue(1.0, "First"));
        list.addCue(Cue(3.0, "Third"));
        list.insertCue(1, Cue(2.0, "Second"));

        QCOMPARE(list.count(), 3);
        QCOMPARE(list.at(1).name(), QString("Second"));
    }

    void testRemoveCue() {
        CueList list;
        QSignalSpy spy(&list, &CueList::cueRemoved);

        list.addCue(Cue(1.0, "First"));
        list.addCue(Cue(2.0, "Second"));
        list.removeCue(0);

        QCOMPARE(list.count(), 1);
        QCOMPARE(list.at(0).name(), QString("Second"));
        QCOMPARE(spy.count(), 1);
    }

    void testFindById() {
        CueList list;
        Cue cue(1.0, "Test");
        QString id = cue.id();
        list.addCue(cue);

        const Cue* found = list.findById(id);
        QVERIFY(found != nullptr);
        QCOMPARE(found->name(), QString("Test"));

        QVERIFY(list.findById("nonexistent") == nullptr);
    }

    void testFindByNumber() {
        CueList list;
        list.addCue(Cue(1.0, "One"));
        list.addCue(Cue(2.5, "TwoHalf"));
        list.addCue(Cue(3.0, "Three"));

        Cue* found = list.findByNumber(2.5);
        QVERIFY(found != nullptr);
        QCOMPARE(found->name(), QString("TwoHalf"));
    }

    void testMoveCue() {
        CueList list;
        QSignalSpy spy(&list, &CueList::cueMoved);

        list.addCue(Cue(1.0, "First"));
        list.addCue(Cue(2.0, "Second"));
        list.addCue(Cue(3.0, "Third"));

        list.moveCue(0, 2);

        QCOMPARE(list.at(0).name(), QString("Second"));
        QCOMPARE(list.at(1).name(), QString("Third"));
        QCOMPARE(list.at(2).name(), QString("First"));
        QCOMPARE(spy.count(), 1);
    }

    void testSortByNumber() {
        CueList list;
        list.addCue(Cue(3.0, "Three"));
        list.addCue(Cue(1.0, "One"));
        list.addCue(Cue(2.0, "Two"));

        list.sortByNumber();

        QCOMPARE(list.at(0).number(), 1.0);
        QCOMPARE(list.at(1).number(), 2.0);
        QCOMPARE(list.at(2).number(), 3.0);
    }

    void testNextCueNumber() {
        CueList list;
        QCOMPARE(list.nextCueNumber(), 1.0);

        list.addCue(Cue(1.0));
        QCOMPARE(list.nextCueNumber(), 2.0);

        list.addCue(Cue(5.5));
        QCOMPARE(list.nextCueNumber(), 6.0);
    }

    void testJsonSerialization() {
        CueList original;
        original.addCue(Cue(1.0, "First"));
        original.addCue(Cue(2.0, "Second"));

        QJsonArray json = original.toJson();

        CueList loaded;
        loaded.fromJson(json);

        QCOMPARE(loaded.count(), 2);
        QCOMPARE(loaded.at(0).name(), QString("First"));
        QCOMPARE(loaded.at(1).name(), QString("Second"));
    }

    void testClear() {
        CueList list;
        QSignalSpy spy(&list, &CueList::listCleared);

        list.addCue(Cue(1.0));
        list.addCue(Cue(2.0));
        list.clear();

        QVERIFY(list.isEmpty());
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestCueList)
#include "test_cuelist.moc"
