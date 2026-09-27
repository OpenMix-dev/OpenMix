#include "protocol/transport/TcpTransport.h"
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {

// a console's TCP port: accepts, keeps the sockets, and can drop or refuse
class StubServer : public QObject {
    Q_OBJECT

  public:
    explicit StubServer(QObject* parent = nullptr) : QObject(parent) {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                m_clients.append(s);
                ++m_accepted;
            }
        });
    }

    quint16 port() const { return m_port ? m_port : m_server.serverPort(); }
    int accepted() const { return m_accepted; }

    // close every accepted socket: the console went away
    void dropClients() {
        for (QTcpSocket* s : m_clients) {
            s->disconnectFromHost();
        }
        m_clients.clear();
    }

    // stop listening: further connects are refused
    void refuse() {
        m_port = m_server.serverPort();
        m_server.close();
    }

    void listenAgain() {
        m_server.listen(QHostAddress::LocalHost, m_port);
        m_port = 0;
    }

  private:
    QTcpServer m_server;
    QList<QTcpSocket*> m_clients;
    int m_accepted = 0;
    quint16 m_port = 0;
};

// a port nothing listens on
quint16 closedPort() {
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

} // namespace

class TestTcpTransport : public QObject {
    Q_OBJECT

  private slots:
    void refusedConnect_endsDisconnectedWithoutRetrying() {
        TcpTransport t;
        t.setReconnectDelay(20);
        QSignalSpy errorSpy(&t, &TcpTransport::connectionError);
        QSignalSpy disconnectedSpy(&t, &TcpTransport::disconnected);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);
        QSignalSpy lostSpy(&t, &TcpTransport::connectionLost);

        QVERIFY(t.connect("127.0.0.1", closedPort()));
        QVERIFY(disconnectedSpy.wait(3000));

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), QStringLiteral("Connection refused"));
        QCOMPARE(disconnectedSpy.count(), 1);
        QCOMPARE(reconnectSpy.count(), 0);
        QCOMPARE(lostSpy.count(), 0);
        QVERIFY(!t.isConnected());

        // and it stays that way: no retry sneaks in afterwards
        QTest::qWait(150);
        QCOMPARE(reconnectSpy.count(), 0);
    }

    void connectTimeout_endsDisconnectedWithoutRetrying() {
        // 10.255.255.1 is unroutable on a normal LAN; the timeout has to fire
        // before the OS gives up on its own
        TcpTransport t;
        t.setConnectionTimeout(200);
        t.setReconnectDelay(20);
        QSignalSpy errorSpy(&t, &TcpTransport::connectionError);
        QSignalSpy disconnectedSpy(&t, &TcpTransport::disconnected);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);

        QVERIFY(t.connect("10.255.255.1", 51325));
        QVERIFY(disconnectedSpy.wait(3000));

        QVERIFY(errorSpy.count() >= 1);
        QCOMPARE(disconnectedSpy.count(), 1);
        QCOMPARE(reconnectSpy.count(), 0);
        QVERIFY(!t.isConnected());
    }

    void userDisconnect_isSilentAndNeverReconnects() {
        StubServer server;
        TcpTransport t;
        t.setReconnectDelay(20);
        QSignalSpy connectedSpy(&t, &TcpTransport::connected);
        QSignalSpy disconnectedSpy(&t, &TcpTransport::disconnected);
        QSignalSpy lostSpy(&t, &TcpTransport::connectionLost);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);

        QVERIFY(t.connect("127.0.0.1", server.port()));
        QVERIFY(connectedSpy.wait(3000));
        QVERIFY(t.isConnected());

        t.disconnect();
        QVERIFY(!t.isConnected());

        // long enough for the backoff to have fired if it were armed
        QTest::qWait(200);
        QCOMPARE(lostSpy.count(), 0);
        QCOMPARE(reconnectSpy.count(), 0);
        QCOMPARE(disconnectedSpy.count(), 0);
        QCOMPARE(server.accepted(), 1);
    }

    void remoteDrop_reconnects() {
        StubServer server;
        TcpTransport t;
        t.setReconnectDelay(20);
        QSignalSpy connectedSpy(&t, &TcpTransport::connected);
        QSignalSpy lostSpy(&t, &TcpTransport::connectionLost);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);
        QSignalSpy disconnectedSpy(&t, &TcpTransport::disconnected);

        QVERIFY(t.connect("127.0.0.1", server.port()));
        QVERIFY(connectedSpy.wait(3000));

        server.dropClients();
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 2, 3000);

        QCOMPARE(lostSpy.count(), 1);
        QCOMPARE(reconnectSpy.count(), 1);
        QCOMPARE(reconnectSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(disconnectedSpy.count(), 0);
        QVERIFY(t.isConnected());
        QCOMPARE(server.accepted(), 2);
    }

    void refusedDuringReconnect_keepsTryingThenGivesUp() {
        StubServer server;
        TcpTransport t;
        t.setReconnectDelay(20);
        t.setMaxReconnectAttempts(3);
        QSignalSpy connectedSpy(&t, &TcpTransport::connected);
        QSignalSpy lostSpy(&t, &TcpTransport::connectionLost);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);
        QSignalSpy errorSpy(&t, &TcpTransport::connectionError);
        QSignalSpy disconnectedSpy(&t, &TcpTransport::disconnected);

        QVERIFY(t.connect("127.0.0.1", server.port()));
        QVERIFY(connectedSpy.wait(3000));

        // the console reboots: it closes us and stops listening
        server.refuse();
        server.dropClients();

        QVERIFY(disconnectedSpy.wait(5000));

        QCOMPARE(lostSpy.count(), 1);
        // every attempt was made, each one refused, then the link gave up
        QCOMPARE(reconnectSpy.count(), 3);
        QCOMPARE(reconnectSpy.at(2).at(0).toInt(), 3);
        QVERIFY(errorSpy.count() >= 3);
        QCOMPARE(errorSpy.last().at(0).toString(),
                 QStringLiteral("Failed to reconnect after maximum attempts"));
        QCOMPARE(disconnectedSpy.count(), 1);
        QVERIFY(!t.isConnected());
    }

    void refusedThenBack_reconnectsOnALaterAttempt() {
        StubServer server;
        TcpTransport t;
        t.setReconnectDelay(20);
        t.setMaxReconnectAttempts(3);
        QSignalSpy connectedSpy(&t, &TcpTransport::connected);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);
        QSignalSpy disconnectedSpy(&t, &TcpTransport::disconnected);

        QVERIFY(t.connect("127.0.0.1", server.port()));
        QVERIFY(connectedSpy.wait(3000));

        server.refuse();
        server.dropClients();
        // first attempt is refused; the console is back before the second
        QTRY_COMPARE_WITH_TIMEOUT(reconnectSpy.count(), 1, 3000);
        server.listenAgain();

        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 2, 3000);
        QCOMPARE(disconnectedSpy.count(), 0);
        QVERIFY(t.isConnected());
    }

    void connectWhileConnected_replacesTheLinkQuietly() {
        StubServer a;
        StubServer b;
        TcpTransport t;
        t.setReconnectDelay(20);
        QSignalSpy connectedSpy(&t, &TcpTransport::connected);
        QSignalSpy lostSpy(&t, &TcpTransport::connectionLost);
        QSignalSpy reconnectSpy(&t, &TcpTransport::reconnecting);

        QVERIFY(t.connect("127.0.0.1", a.port()));
        QVERIFY(connectedSpy.wait(3000));
        QVERIFY(t.connect("127.0.0.1", b.port()));
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 2, 3000);

        QTest::qWait(150);
        QCOMPARE(lostSpy.count(), 0);
        QCOMPARE(reconnectSpy.count(), 0);
        QCOMPARE(a.accepted(), 1);
        QCOMPARE(b.accepted(), 1);
        QCOMPARE(t.port(), static_cast<int>(b.port()));
    }
};

QTEST_MAIN(TestTcpTransport)
#include "test_tcp_transport.moc"
