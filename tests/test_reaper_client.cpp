#include "app/ReaperClient.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestReaperClient : public QObject {
    Q_OBJECT

  private slots:
    void recordModeDropsAndNamesMarker() {
        ReaperClient c;
        c.setTarget("127.0.0.1", 8000);
        c.setEnabled(true);
        c.setRecordMode(true);

        QSignalSpy sent(&c, &ReaperClient::sent);
        c.onCueFired(1.0, "Opening");

        QCOMPARE(sent.count(), 2);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/action/40157")); // insert marker
        QCOMPARE(sent.at(1).at(0).toString(), QString("/lastmarker/name"));
    }

    void playbackJumpsToRecordedMarker() {
        ReaperClient c;
        c.setTarget("127.0.0.1", 8000);
        c.setEnabled(true);

        c.setRecordMode(true);
        c.onCueFired(5.0, "Five"); // records marker index 1 for cue 5

        c.setRecordMode(false);
        QSignalSpy sent(&c, &ReaperClient::sent);
        c.onCueFired(5.0, "Five"); // playback -> jump to marker 1

        QCOMPARE(sent.count(), 1);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/marker/1"));
    }

    void playbackUnknownCueIsNoOp() {
        ReaperClient c;
        c.setTarget("127.0.0.1", 8000);
        c.setEnabled(true);
        c.setRecordMode(false);

        QSignalSpy sent(&c, &ReaperClient::sent);
        c.onCueFired(9.0, "Nine"); // never recorded
        QCOMPARE(sent.count(), 0);
    }

    void disabledSendsNothing() {
        ReaperClient c;
        c.setTarget("127.0.0.1", 8000);
        c.setRecordMode(true);

        QSignalSpy sent(&c, &ReaperClient::sent);
        c.onCueFired(1.0, "x");
        QCOMPARE(sent.count(), 0);
    }
};

QTEST_MAIN(TestReaperClient)
#include "test_reaper_client.moc"
