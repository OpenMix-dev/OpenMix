#include "app/OscRemoteServer.h"
#include <QByteArray>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <lo/lo.h>

using namespace OpenMix;

namespace {
lo_address addrFor(const OscRemoteServer& server) {
    return lo_address_new("127.0.0.1", QByteArray::number(server.port()).constData());
}
} // namespace

class TestOscRemote : public QObject {
    Q_OBJECT

  private slots:
    void startsOnFreePort() {
        OscRemoteServer server;
        QVERIFY(server.start(0));
        QVERIFY(server.port() > 0);
        QVERIFY(server.isRunning());
        server.stop();
        QVERIFY(!server.isRunning());
    }

    void cueGoEmitsGoRequested() {
        OscRemoteServer server;
        QVERIFY(server.start(0));
        QSignalSpy spy(&server, &OscRemoteServer::goRequested);

        lo_address addr = addrFor(server);
        lo_send(addr, "/cue/go", "");
        QVERIFY(spy.wait(2000));
        QCOMPARE(spy.count(), 1);
        lo_address_free(addr);
    }

    void gotoCarriesCueNumber() {
        OscRemoteServer server;
        QVERIFY(server.start(0));
        QSignalSpy spy(&server, &OscRemoteServer::gotoRequested);

        lo_address addr = addrFor(server);
        lo_send(addr, "/cue/goto", "f", 12.5f);
        QVERIFY(spy.wait(2000));
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 12.5) < 1e-6);
        lo_address_free(addr);
    }

    void fadeAllAndStopAndNext() {
        OscRemoteServer server;
        QVERIFY(server.start(0));
        QSignalSpy fade(&server, &OscRemoteServer::fadeAllRequested);
        QSignalSpy stop(&server, &OscRemoteServer::stopRequested);
        QSignalSpy next(&server, &OscRemoteServer::nextRequested);

        lo_address addr = addrFor(server);
        lo_send(addr, "/ctrl/fadeall", "");
        QVERIFY(fade.wait(2000));
        lo_send(addr, "/ctrl/stop", "");
        QVERIFY(stop.wait(2000));
        lo_send(addr, "/next", "");
        QVERIFY(next.wait(2000));
        lo_address_free(addr);
    }
};

QTEST_MAIN(TestOscRemote)
#include "test_osc_remote.moc"
