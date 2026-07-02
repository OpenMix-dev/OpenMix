#include "app/ScsClient.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestScsClient : public QObject {
    Q_OBJECT

  private slots:
    void transportButtonsSendCtrlAddresses() {
        ScsClient c;
        c.setTarget("127.0.0.1", 58968);
        c.setEnabled(true);

        QSignalSpy sent(&c, &ScsClient::sent);
        c.go();
        c.stop();
        c.fade();
        c.pauseResume();

        QCOMPARE(sent.count(), 4);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/ctrl/go"));
        QCOMPARE(sent.at(1).at(0).toString(), QString("/ctrl/stopall"));
        QCOMPARE(sent.at(2).at(0).toString(), QString("/ctrl/fadeall"));
        QCOMPARE(sent.at(3).at(0).toString(), QString("/ctrl/pauseresumeall"));
    }

    void fireCueSendsCueGo() {
        ScsClient c;
        c.setTarget("127.0.0.1", 58968);
        c.setEnabled(true);

        QSignalSpy sent(&c, &ScsClient::sent);
        c.fireCue("Q1");

        QCOMPARE(sent.count(), 1);
        QCOMPARE(sent.at(0).at(0).toString(), QString("/cue/go"));
    }

    void defaultPortIsScs() {
        ScsClient c;
        QCOMPARE(c.port(), 58968);
    }

    void disabledSendsNothing() {
        ScsClient c;
        c.setTarget("127.0.0.1", 58968);
        QSignalSpy sent(&c, &ScsClient::sent);
        c.go();
        c.fireCue("Q1");
        QCOMPARE(sent.count(), 0);
    }
};

QTEST_MAIN(TestScsClient)
#include "test_scs_client.moc"
