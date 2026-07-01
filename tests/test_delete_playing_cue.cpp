#include "core/Cue.h"
#include "core/CueList.h"
#include "ui/CueFilterProxyModel.h"
#include "ui/CueTableModel.h"
#include <QItemSelectionModel>
#include <QTableView>
#include <QtTest/QtTest>

using namespace OpenMix;

// Deleting the cue that is currently playing (highlighted + selected) used to
// crash because the model bracketed the row removal after the data was already
// gone. This exercises the model/proxy/view path to guard against regression.
class TestDeletePlayingCue : public QObject {
    Q_OBJECT

  private slots:
    void deleteCurrentSelectedRow() {
        CueList list;
        for (int i = 1; i <= 5; ++i) {
            Cue c;
            c.setNumber(i);
            list.addCue(c);
        }

        CueTableModel model(&list);
        CueFilterProxyModel proxy;
        proxy.setSourceModel(&model);

        QTableView view;
        view.setModel(&proxy);

        // mark row 2 as the playing cue and select it
        model.setCurrentCueIndex(2);
        model.setStandbyCueIndex(3);
        view.selectRow(2);

        // delete the playing cue
        list.removeCue(2);
        QCoreApplication::processEvents();

        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(model.currentCueIndex(), -1); // playing cue is gone
        QCOMPARE(model.standbyCueIndex(), 2);  // shifted down by one
    }

    void deleteRowBeforeCurrent() {
        CueList list;
        for (int i = 1; i <= 4; ++i) {
            Cue c;
            c.setNumber(i);
            list.addCue(c);
        }
        CueTableModel model(&list);
        model.setCurrentCueIndex(3);
        list.removeCue(1);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.currentCueIndex(), 2); // shifted down
    }
};

QTEST_MAIN(TestDeletePlayingCue)
#include "test_delete_playing_cue.moc"
