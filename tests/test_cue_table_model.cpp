#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/DCAMapping.h"
#include "ui/CueTableModel.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestCueTableModel : public QObject {
    Q_OBJECT

    // first column of the DCA n triplet (the assignment cell)
    static int dcaCol(const CueTableModel& model, int dca) {
        return CueTableModel::ColCount + (dca - 1) * CueTableModel::DcaSubCols;
    }

  private slots:
    // --- dcaCount / columns ------------------------------------------------
    void columnCount_followsDcaCount() {
        CueList list;
        CueTableModel model(&list);
        QCOMPARE(model.columnCount(), int(CueTableModel::ColCount) + 8 * 3);

        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.setDcaCount(24);
        QCOMPARE(reset.count(), 1);
        QCOMPARE(model.columnCount(), int(CueTableModel::ColCount) + 24 * 3);

        model.setDcaCount(24); // same value -> no reset
        QCOMPARE(reset.count(), 1);
    }

    void dcaColumnMapping_atBoundaries() {
        CueList list;
        CueTableModel model(&list);
        model.setDcaCount(24);
        QCOMPARE(model.dcaOfColumn(CueTableModel::ColCount), 1);
        QCOMPARE(model.dcaSubColumn(CueTableModel::ColCount), 0);
        const int lastCol = CueTableModel::ColCount + 24 * 3 - 1;
        QCOMPARE(model.dcaOfColumn(lastCol), 24);
        QCOMPARE(model.dcaSubColumn(lastCol), 2);
        QCOMPARE(model.dcaOfColumn(lastCol + 1), -1);
        QCOMPARE(model.dcaOfColumn(CueTableModel::ColCount - 1), -1);
    }

    void dcaCount_clamps() {
        CueList list;
        CueTableModel model(&list);
        model.setDcaCount(-5);
        QCOMPARE(model.dcaCount(), 0);
        model.setDcaCount(100);
        QCOMPARE(model.dcaCount(), 64);
    }

    // --- DCA assignment cells ----------------------------------------------
    void dcaAssignmentCells_areEditable_fxPosAreNot() {
        CueList list;
        list.addCue(Cue(1.0, "One"));
        CueTableModel model(&list);

        const QModelIndex assign = model.index(0, dcaCol(model, 1));
        QVERIFY(model.flags(assign) & Qt::ItemIsEditable);
        QVERIFY(!(model.flags(model.index(0, dcaCol(model, 1) + 1)) & Qt::ItemIsEditable));
        QVERIFY(!(model.flags(model.index(0, dcaCol(model, 1) + 2)) & Qt::ItemIsEditable));
    }

    void setData_resolvingRole_assignsChannel() {
        CueList list;
        list.addCue(Cue(1.0, "One"));
        DCAMapping mapping;
        mapping.assignChannelToDCA(9, 2); // pre-existing show mapping on DCA 2

        ActorProfileLibrary library;
        Actor alice("Alice", 5);
        alice.setRoles({"Cosette"});
        library.addActor(alice);

        CueTableModel model(&list);
        model.setDcaMapping(&mapping);
        model.setActorLibrary(&library);

        QVERIFY(model.setData(model.index(0, dcaCol(model, 1)), "Cosette", Qt::EditRole));

        const Cue& cue = list.at(0);
        QCOMPARE(cue.dcaOverride(1).label, std::optional<QString>("Cosette"));
        // the actor's channel landed in the cue's custom mapping for DCA 1...
        QVERIFY(cue.hasCustomDCAMapping());
        QVERIFY(cue.dcaChannelMapping().value(1).contains(5));
        // ...and the seeded show mapping was preserved
        QVERIFY(cue.dcaChannelMapping().value(2).contains(9));
    }

    void setData_nonResolvingText_storesLabelOnly() {
        CueList list;
        list.addCue(Cue(1.0, "One"));
        CueTableModel model(&list);

        QVERIFY(model.setData(model.index(0, dcaCol(model, 1)), "Band", Qt::EditRole));
        QCOMPARE(list.at(0).dcaOverride(1).label, std::optional<QString>("Band"));
        QVERIFY(!list.at(0).hasCustomDCAMapping());
    }

    void setData_emptyText_clearsLabel_keepsMute() {
        CueList list;
        Cue cue(1.0, "One");
        DCAOverride ov;
        ov.mute = true;
        ov.label = "Old";
        cue.setDCAOverride(1, ov);
        list.addCue(cue);

        CueTableModel model(&list);
        QVERIFY(model.setData(model.index(0, dcaCol(model, 1)), "", Qt::EditRole));
        QVERIFY(!list.at(0).dcaOverride(1).label.has_value());
        QCOMPARE(list.at(0).dcaOverride(1).mute, std::optional<bool>(true));
    }

    void display_resolvesActorInRole_editReturnsRawLabel() {
        CueList list;
        Cue cue(1.0, "One");
        DCAOverride ov;
        ov.label = "Cosette";
        cue.setDCAOverride(1, ov);
        list.addCue(cue);

        ActorProfileLibrary library;
        Actor alice("Alice", 5);
        alice.setRoles({"Cosette"});
        library.addActor(alice);

        CueTableModel model(&list);
        model.setActorLibrary(&library);

        const QModelIndex idx = model.index(0, dcaCol(model, 1));
        QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QString("Alice (Cosette)"));
        QCOMPARE(model.data(idx, Qt::EditRole).toString(), QString("Cosette"));
    }

    void display_muteOnlyOverride() {
        CueList list;
        Cue cue(1.0, "One");
        DCAOverride ov;
        ov.mute = true;
        cue.setDCAOverride(1, ov);
        list.addCue(cue);

        CueTableModel model(&list);
        QCOMPARE(model.data(model.index(0, dcaCol(model, 1)), Qt::DisplayRole).toString(),
                 QString("mute"));
    }

    void tooltip_listsResolvedMembers() {
        CueList list;
        Cue cue(1.0, "One");
        DCAOverride ov;
        ov.label = "Cosette";
        cue.setDCAOverride(1, ov);
        list.addCue(cue);

        DCAMapping mapping;
        mapping.assignChannelToDCA(5, 1);

        ActorProfileLibrary library;
        Actor alice("Alice", 5);
        alice.setRoles({"Cosette"});
        library.addActor(alice);

        CueTableModel model(&list);
        model.setDcaMapping(&mapping);
        model.setActorLibrary(&library);

        const QString tip =
            model.data(model.index(0, dcaCol(model, 1)), Qt::ToolTipRole).toString();
        QVERIFY(tip.contains("Ch 5 Alice (Cosette)"));
        QVERIFY(tip.contains("Label: Cosette"));
    }

    void cueUpdate_repaintSpansDcaColumns() {
        CueList list;
        list.addCue(Cue(1.0, "One"));
        CueTableModel model(&list);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        Cue cue = list.at(0);
        cue.setName("Renamed");
        list.updateCue(0, cue);

        QVERIFY(changed.count() >= 1);
        const QModelIndex bottomRight = changed.last().at(1).toModelIndex();
        QCOMPARE(bottomRight.column(), model.columnCount() - 1); // includes DCA columns
    }
};

QTEST_MAIN(TestCueTableModel)
#include "test_cue_table_model.moc"
