#include "protocol/MixerCapabilities.h"
#include "protocol/behringer/WingProtocol.h"
#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest/QtTest>
#include <cstdlib>
#include <lo/lo.h>

using namespace OpenMix;

namespace {

QByteArray oscBytes(const char* path, lo_message msg) {
    size_t len = 0;
    void* buf = lo_message_serialise(msg, path, nullptr, &len);
    QByteArray out(static_cast<const char*>(buf), static_cast<int>(len));
    std::free(buf);
    return out;
}

QByteArray oscString(const char* path, const char* value) {
    lo_message msg = lo_message_new();
    lo_message_add_string(msg, value);
    const QByteArray out = oscBytes(path, msg);
    lo_message_free(msg);
    return out;
}

// the address pattern of a datagram, i.e. everything before the first NUL
QByteArray oscPath(const QByteArray& datagram) { return datagram.left(datagram.indexOf('\0')); }

// stands in for the console's OSC server on 2223: answers "/?" like a WING
// and keeps every datagram it was sent
class StubWing : public QObject {
    Q_OBJECT

  public:
    explicit StubWing(QObject* parent = nullptr) : QObject(parent) {
        m_socket.bind(QHostAddress::LocalHost, 0);
        connect(&m_socket, &QUdpSocket::readyRead, this, [this]() {
            while (m_socket.hasPendingDatagrams()) {
                const QNetworkDatagram in = m_socket.receiveDatagram();
                m_client = in.senderAddress();
                m_clientPort = static_cast<quint16>(in.senderPort());
                m_received.append(in.data());
                if (m_answering && oscPath(in.data()) == "/?") {
                    m_socket.writeDatagram(
                        oscString("/?", "WING,127.0.0.1,STUB,ngc-full,NO_SERIAL,1.07.2"), m_client,
                        m_clientPort);
                }
            }
        });
    }

    quint16 port() const { return m_socket.localPort(); }
    const QList<QByteArray>& received() const { return m_received; }
    void setAnswering(bool on) { m_answering = on; }

    int count(const QByteArray& path) const {
        int n = 0;
        for (const QByteArray& d : m_received) {
            if (oscPath(d) == path) {
                ++n;
            }
        }
        return n;
    }

    // a read reply the way the console sends it: ",sff" string, normalised, real
    void sendFader(const char* node, const char* text, float normalised, float dB) {
        lo_message msg = lo_message_new();
        lo_message_add_string(msg, text);
        lo_message_add_float(msg, normalised);
        lo_message_add_float(msg, dB);
        m_socket.writeDatagram(oscBytes(node, msg), m_client, m_clientPort);
        lo_message_free(msg);
    }

    void sendMute(const char* node, const char* text, float normalised, int value) {
        lo_message msg = lo_message_new();
        lo_message_add_string(msg, text);
        lo_message_add_float(msg, normalised);
        lo_message_add_int32(msg, value);
        m_socket.writeDatagram(oscBytes(node, msg), m_client, m_clientPort);
        lo_message_free(msg);
    }

  private:
    QUdpSocket m_socket;
    QHostAddress m_client;
    quint16 m_clientPort = 0;
    QList<QByteArray> m_received;
    bool m_answering = true;
};

} // namespace

class TestWingProtocol : public QObject {
    Q_OBJECT

  private slots:
    void connect_probesWithInfoAndSubscribesWithTheRealAddress() {
        StubWing wing;
        WingProtocol p(MixerCapabilities::forConsole(ConsoleType::Wing));
        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);

        QVERIFY(p.connect("127.0.0.1", wing.port()));
        QVERIFY(connectedSpy.wait(3000));
        QVERIFY(p.isConnected());

        // the very first datagram is the info probe, sent as a bare address
        QCOMPARE(oscPath(wing.received().first()), QByteArray("/?"));

        // the subscription goes out on connect, and its address is "/*s" with a
        // NUL terminator: the doc's "~" is notation for the NUL, not a byte
        QTRY_VERIFY_WITH_TIMEOUT(wing.count("/*s") >= 1, 2000);
        QByteArray subscribe;
        for (const QByteArray& d : wing.received()) {
            if (oscPath(d) == "/*s") {
                subscribe = d;
                break;
            }
        }
        QCOMPARE(subscribe, QByteArray("/*s\0,\0\0\0", 8));
        QCOMPARE(wing.count("/*s~"), 0);
    }

    void reads_useTheRealWorldArgumentOfTheTriplet() {
        StubWing wing;
        WingProtocol p(MixerCapabilities::forConsole(ConsoleType::Wing));
        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QSignalSpy paramSpy(&p, &MixerProtocol::parameterChanged);

        QVERIFY(p.connect("127.0.0.1", wing.port()));
        QVERIFY(connectedSpy.wait(3000));

        // W-> /ch/2/fdr ,sff "-2.0" [0.7000] [-2.0000]
        wing.sendFader("/ch/2/fdr", "-2.0", 0.7f, -2.0f);
        QTRY_VERIFY_WITH_TIMEOUT(paramSpy.count() >= 1, 2000);
        QCOMPARE(paramSpy.last().at(0).toString(), QStringLiteral("/ch/2/fdr"));
        QCOMPARE(paramSpy.last().at(1).toFloat(), -2.0f);

        // W-> /ch/1/mute ,sfi "1" [1.0000] [1]
        wing.sendMute("/ch/1/mute", "1", 1.0f, 1);
        QTRY_VERIFY_WITH_TIMEOUT(paramSpy.count() >= 2, 2000);
        QCOMPARE(paramSpy.last().at(0).toString(), QStringLiteral("/ch/1/mute"));
        QCOMPARE(paramSpy.last().at(1).toInt(), 1);

        // and the cache reads back in real units, so DCA masks etc. are numbers
        QCOMPARE(p.getParameter("/ch/2/fdr").toFloat(), -2.0f);
    }

    void idleConsole_staysConnectedWhileItAnswersProbes() {
        StubWing wing;
        WingProtocol p(MixerCapabilities::forConsole(ConsoleType::Wing));
        p.setKeepAliveInterval(100);
        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QSignalSpy lostSpy(&p, &MixerProtocol::connectionLost);

        QVERIFY(p.connect("127.0.0.1", wing.port()));
        QVERIFY(connectedSpy.wait(3000));

        // nothing changes on the desk for ten keep-alive periods: the driver
        // must keep renewing and probing, and must not call the link dead
        QTest::qWait(1000);
        QVERIFY(wing.count("/*s") >= 5);
        QVERIFY(wing.count("/?") >= 5);
        QCOMPARE(lostSpy.count(), 0);
        QVERIFY(p.isConnected());
    }

    void silentConsole_isReportedLostAfterThreeMissedProbes() {
        StubWing wing;
        WingProtocol p(MixerCapabilities::forConsole(ConsoleType::Wing));
        p.setKeepAliveInterval(100);
        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QSignalSpy lostSpy(&p, &MixerProtocol::connectionLost);

        QVERIFY(p.connect("127.0.0.1", wing.port()));
        QVERIFY(connectedSpy.wait(3000));

        wing.setAnswering(false);
        QVERIFY(lostSpy.wait(3000));
        QCOMPARE(p.connectionState(), ConnectionState::Reconnecting);
    }

    void disconnect_isQuiet() {
        StubWing wing;
        WingProtocol p(MixerCapabilities::forConsole(ConsoleType::Wing));
        p.setKeepAliveInterval(100);
        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QSignalSpy lostSpy(&p, &MixerProtocol::connectionLost);
        QList<ConnectionState> states;
        connect(&p, &MixerProtocol::connectionStateChanged, this,
                [&states](ConnectionState s) { states.append(s); });

        QVERIFY(p.connect("127.0.0.1", wing.port()));
        QVERIFY(connectedSpy.wait(3000));

        p.disconnect();
        QTest::qWait(400);
        QCOMPARE(lostSpy.count(), 0);
        QCOMPARE(p.connectionState(), ConnectionState::Disconnected);
        // never passed through Reconnecting on the way down
        QVERIFY(!states.contains(ConnectionState::Reconnecting));
    }
};

QTEST_MAIN(TestWingProtocol)
#include "test_wing_protocol.moc"
