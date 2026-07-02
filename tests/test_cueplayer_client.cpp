#include "app/CuePlayerClient.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestCuePlayerClient : public QObject {
    Q_OBJECT

  private slots:
    void playAndStopSendCpsound() {
        CuePlayerClient c;
        c.setTarget("127.0.0.1", 50550);
        c.setEnabled(true);

        QSignalSpy sent(&c, &CuePlayerClient::sent);
        c.play();
        c.stop();

        QCOMPARE(sent.count(), 2);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/cpsound/play"));
        QCOMPARE(sent.at(1).at(0).toString(), QString("/cpsound/stop"));
    }

    void disabledSendsNothing() {
        CuePlayerClient c;
        c.setTarget("127.0.0.1", 50550);
        QSignalSpy sent(&c, &CuePlayerClient::sent);
        c.play();
        QCOMPARE(sent.count(), 0);
    }
};

QTEST_MAIN(TestCuePlayerClient)
#include "test_cueplayer_client.moc"
