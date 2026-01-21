#include "core/Cue.h"
#include "core/CueList.h"
#include "core/LiveEditSession.h"
#include "core/PreviewLayer.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestLiveEditSession : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        // set up common test fixtures
    }

    void testInitialState() {
        LiveEditSession session;
        QCOMPARE(session.mode(), LiveEditMode::Inactive);
        QVERIFY(!session.isActive());
        QVERIFY(!session.isPreview());
        QVERIFY(!session.isLive());
        QVERIFY(session.activeCueId().isEmpty());
        QVERIFY(!session.hasEdits());
        QCOMPARE(session.editCount(), 0);
    }

    void testStartLiveEdit() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue);

        session.setCueList(&cueList);

        QSignalSpy modeChangedSpy(&session, &LiveEditSession::modeChanged);
        QSignalSpy sessionStartedSpy(&session, &LiveEditSession::sessionStarted);

        session.startLiveEdit(cue.id());

        QCOMPARE(session.mode(), LiveEditMode::Live);
        QVERIFY(session.isActive());
        QVERIFY(session.isLive());
        QVERIFY(!session.isPreview());
        QCOMPARE(session.activeCueId(), cue.id());

        QCOMPARE(modeChangedSpy.count(), 1);
        QCOMPARE(sessionStartedSpy.count(), 1);
    }

    void testStartPreview() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cueList.addCue(cue);

        session.setCueList(&cueList);

        session.startPreview(cue.id());

        QCOMPARE(session.mode(), LiveEditMode::Preview);
        QVERIFY(session.isActive());
        QVERIFY(session.isPreview());
        QVERIFY(!session.isLive());
    }

    void testTogglePreviewMode() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        QCOMPARE(session.mode(), LiveEditMode::Live);

        session.togglePreviewMode();
        QCOMPARE(session.mode(), LiveEditMode::Preview);

        session.togglePreviewMode();
        QCOMPARE(session.mode(), LiveEditMode::Live);
    }

    void testSetParameter() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        QSignalSpy paramEditedSpy(&session, &LiveEditSession::parameterEdited);

        session.setParameter("/dca/1/fader", 0.75);

        QVERIFY(session.hasEdits());
        QCOMPARE(session.editCount(), 1);
        QVERIFY(session.editedPaths().contains("/dca/1/fader"));

        const ParameterEdit* edit = session.edit("/dca/1/fader");
        QVERIFY(edit != nullptr);
        QCOMPARE(edit->currentValue.toDouble(), 0.75);
        QCOMPARE(edit->originalValue.toDouble(), 0.5);

        QCOMPARE(paramEditedSpy.count(), 1);
    }

    void testRevertParameter() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        session.setParameter("/dca/1/fader", 0.75);
        QVERIFY(session.hasEdits());

        QSignalSpy paramRevertedSpy(&session, &LiveEditSession::parameterReverted);

        session.revertParameter("/dca/1/fader");

        QVERIFY(!session.hasEdits());
        QCOMPARE(session.editCount(), 0);
        QCOMPARE(paramRevertedSpy.count(), 1);
    }

    void testRevertAll() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cue.setParameter("/dca/2/fader", 0.6);
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        session.setParameter("/dca/1/fader", 0.75);
        session.setParameter("/dca/2/fader", 0.85);
        QCOMPARE(session.editCount(), 2);

        session.revertAll();

        QVERIFY(!session.hasEdits());
        QCOMPARE(session.editCount(), 0);
    }

    void testCancel() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        session.setParameter("/dca/1/fader", 0.75);

        QSignalSpy cancelledSpy(&session, &LiveEditSession::editsCancelled);
        QSignalSpy endedSpy(&session, &LiveEditSession::sessionEnded);

        session.cancel();

        QCOMPARE(session.mode(), LiveEditMode::Inactive);
        QVERIFY(!session.isActive());
        QVERIFY(!session.hasEdits());
        QVERIFY(session.activeCueId().isEmpty());

        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(endedSpy.count(), 1);
    }

    void testCommitToCurrentCue() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        session.setParameter("/dca/1/fader", 0.75);

        QSignalSpy committedSpy(&session, &LiveEditSession::editsCommitted);
        QSignalSpy endedSpy(&session, &LiveEditSession::sessionEnded);

        session.commitToCurrentCue();

        // verify cue was updated
        const Cue& updatedCue = cueList.at(0);
        QCOMPARE(updatedCue.parameter("/dca/1/fader").toDouble(), 0.75);

        // verify session ended
        QCOMPARE(session.mode(), LiveEditMode::Inactive);
        QVERIFY(!session.isActive());

        QCOMPARE(committedSpy.count(), 1);
        QCOMPARE(endedSpy.count(), 1);
    }

    void testCommitToDifferentCue() {
        LiveEditSession session;
        CueList cueList;

        Cue cue1(1.0, "Cue 1");
        cue1.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue1);

        Cue cue2(2.0, "Cue 2");
        cue2.setParameter("/dca/1/fader", 0.3);
        cueList.addCue(cue2);

        session.setCueList(&cueList);
        session.startLiveEdit(cue1.id());

        session.setParameter("/dca/1/fader", 0.9);

        // commit to cue 2 instead
        session.commitToCue(cue2.id());

        // verify cue 1 was NOT updated
        QCOMPARE(cueList.at(0).parameter("/dca/1/fader").toDouble(), 0.5);

        // verify cue 2 WAS updated
        QCOMPARE(cueList.at(1).parameter("/dca/1/fader").toDouble(), 0.9);
    }

    void testPendingEdits() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        session.setParameter("/dca/1/fader", 0.5);
        session.setParameter("/dca/2/fader", 0.6);

        QJsonObject pending = session.pendingEdits();
        QCOMPARE(pending.count(), 2);
        QCOMPARE(pending["/dca/1/fader"].toDouble(), 0.5);
        QCOMPARE(pending["/dca/2/fader"].toDouble(), 0.6);
    }

    void testOriginalValue() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        // get original value before edits
        QCOMPARE(session.originalValue("/dca/1/fader").toDouble(), 0.5);

        // change value
        session.setParameter("/dca/1/fader", 0.9);

        // original value should still be 0.5
        QCOMPARE(session.originalValue("/dca/1/fader").toDouble(), 0.5);
    }

    void testActiveCueIndex() {
        LiveEditSession session;
        CueList cueList;

        Cue cue1(1.0, "Cue 1");
        cueList.addCue(cue1);

        Cue cue2(2.0, "Cue 2");
        cueList.addCue(cue2);

        session.setCueList(&cueList);

        QCOMPARE(session.activeCueIndex(), -1);

        session.startLiveEdit(cue2.id());
        QCOMPARE(session.activeCueIndex(), 1);

        session.cancel();
        QCOMPARE(session.activeCueIndex(), -1);
    }

    void testStartByIndex() {
        LiveEditSession session;
        CueList cueList;

        Cue cue1(1.0, "Cue 1");
        cueList.addCue(cue1);

        Cue cue2(2.0, "Cue 2");
        cueList.addCue(cue2);

        session.setCueList(&cueList);

        session.startLiveEditByIndex(1);

        QCOMPARE(session.activeCueId(), cue2.id());
        QCOMPARE(session.activeCueIndex(), 1);
    }

    void testInactiveSessionIgnoresEdits() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cueList.addCue(cue);

        session.setCueList(&cueList);

        // try to set parameter without starting session
        session.setParameter("/dca/1/fader", 0.75);

        QVERIFY(!session.hasEdits());
        QCOMPARE(session.editCount(), 0);
    }

    void testMultipleEditsToSameParameter() {
        LiveEditSession session;
        CueList cueList;

        Cue cue(1.0, "Test Cue");
        cue.setParameter("/dca/1/fader", 0.5);
        cueList.addCue(cue);

        session.setCueList(&cueList);
        session.startLiveEdit(cue.id());

        session.setParameter("/dca/1/fader", 0.6);
        session.setParameter("/dca/1/fader", 0.7);
        session.setParameter("/dca/1/fader", 0.8);

        // should still only have one edit entry
        QCOMPARE(session.editCount(), 1);

        // but with the latest value
        const ParameterEdit* edit = session.edit("/dca/1/fader");
        QVERIFY(edit != nullptr);
        QCOMPARE(edit->currentValue.toDouble(), 0.8);

        // and still the original value
        QCOMPARE(edit->originalValue.toDouble(), 0.5);
    }
};

QTEST_MAIN(TestLiveEditSession)
#include "test_liveeditsession.moc"
