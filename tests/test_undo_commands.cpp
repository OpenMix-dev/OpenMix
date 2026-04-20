#include "core/CueList.h"
#include "core/UndoCommands.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestUndoCommands : public QObject {
    Q_OBJECT

  private slots:
    void addCueUndo_onEmptyList_doesNotCrash() {
        CueList list;
        // m_index = -1 means "append"; when list is empty, undo idx = count()-1 = -1
        AddCueCommand cmd(&list, Cue(1.0, "Test"), -1);
        cmd.undo();
        QCOMPARE(list.count(), 0);
    }

    void addCueUndo_removesAddedCue() {
        CueList list;
        list.addCue(Cue(1.0, "Existing"));

        // simulate: cue at index 0 was added before command was pushed
        AddCueCommand cmd(&list, Cue(1.0, "Existing"), 0);
        cmd.undo();
        QCOMPARE(list.count(), 0);
    }

    void batchEditCommand_undoRedo_worksCorrectly() {
        CueList list;
        list.addCue(Cue(1.0, "Old 1"));
        list.addCue(Cue(2.0, "Old 2"));

        QVector<Cue> oldCues = {list.at(0), list.at(1)};

        Cue newCue1 = oldCues[0];
        newCue1.setName("New 1");
        Cue newCue2 = oldCues[1];
        newCue2.setName("New 2");
        QVector<Cue> newCues = {newCue1, newCue2};

        // apply edit externally (simulates what the app does before pushing to stack)
        list.updateCue(0, newCues[0]);
        list.updateCue(1, newCues[1]);

        QVector<int> indices = {0, 1};
        BatchEditCommand cmd(&list, indices, oldCues, newCues, "Batch rename");

        // first redo is a no-op (simulates push to QUndoStack)
        cmd.redo();
        QCOMPARE(list.at(0).name(), QString("New 1"));

        cmd.undo();
        QCOMPARE(list.at(0).name(), QString("Old 1"));
        QCOMPARE(list.at(1).name(), QString("Old 2"));

        cmd.redo();
        QCOMPARE(list.at(0).name(), QString("New 1"));
        QCOMPARE(list.at(1).name(), QString("New 2"));
    }
};

QTEST_MAIN(TestUndoCommands)
#include "test_undo_commands.moc"
