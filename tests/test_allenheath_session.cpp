#include "protocol/MixerCapabilities.h"
#include "protocol/allenheath/DLiveProtocol.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {

// Stands in for a dLive MixRack's ACE control socket, replaying the reference's
// side of the session: stay silent until the client advertises its UDP port with
// E0 00 04 01 03 <port:2> E7, answer with our own port, then hand back a 2-byte
// handle per subscribe (one at a time, as the console does).
class StubMixRack : public QObject {
    Q_OBJECT

  public:
    explicit StubMixRack(QObject* parent = nullptr) : QObject(parent) {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            m_client = m_server.nextPendingConnection();
            connect(m_client, &QTcpSocket::readyRead, this, &StubMixRack::onReadyRead);
        });
    }

    quint16 port() const { return m_server.serverPort(); }
    const QList<QByteArray>& subscribesSeen() const { return m_subscribesSeen; }
    quint16 clientUdpPort() const { return m_clientUdpPort; }
    bool sawSeed() const { return m_sawSeed; }

    // one 8-byte record per channel; only byte 0 is a level
    void sendMeters(const QMap<int, quint8>& levels) {
        QByteArray d = QByteArray::fromHex("f000000006000100020002");
        d.append(QByteArray(METER_CHANNELS * 8, '\0'));
        for (auto it = levels.begin(); it != levels.end(); ++it) {
            d[11 + (it.key() - 1) * 8] = static_cast<char>(it.value());
        }
        QUdpSocket out;
        out.writeDatagram(d, QHostAddress::LocalHost, m_clientUdpPort);
    }

    void sendSceneChanged(int scene) {
        QByteArray f = QByteArray::fromHex("f00000000b000100020002");
        f.append(static_cast<char>(((scene - 1) >> 8) & 0xFF));
        f.append(static_cast<char>((scene - 1) & 0xFF));
        f.append('\xf7');
        m_client->write(f);
    }

    static constexpr int METER_CHANNELS = 128;

    // a console that never answers the port exchange
    void setMute(bool mute) { m_mute = mute; }

  private slots:
    void onReadyRead() {
        m_buffer.append(m_client->readAll());

        while (!m_buffer.isEmpty()) {
            const quint8 lead = static_cast<quint8>(m_buffer.at(0));
            if (lead == 0xE0) {
                const int end = m_buffer.indexOf('\xe7', 1);
                if (end < 0) {
                    return;
                }
                handleControl(m_buffer.left(end + 1));
                m_buffer.remove(0, end + 1);
            } else if (lead == 0xF0) {
                if (m_buffer.size() < 11) {
                    return;
                }
                const int payloadLen = (static_cast<quint8>(m_buffer.at(9)) << 8) |
                                       static_cast<quint8>(m_buffer.at(10));
                const int frameLen = payloadLen + 0x0C;
                if (m_buffer.size() < frameLen) {
                    return;
                }
                handleSubscribe(m_buffer.left(frameLen));
                m_buffer.remove(0, frameLen);
            } else {
                m_buffer.remove(0, 1);
            }
        }
    }

  private:
    void handleControl(const QByteArray& frame) {
        // only the client's 01 seed opens a session; a real MixRack ignores anything else
        if (!frame.startsWith(QByteArray::fromHex("e000040103")) || frame.size() < 8) {
            return;
        }
        m_sawSeed = true;
        m_clientUdpPort = static_cast<quint16>((static_cast<quint8>(frame.at(5)) << 8) |
                                               static_cast<quint8>(frame.at(6)));
        if (m_mute) {
            return;
        }
        QByteArray reply = QByteArray::fromHex("e000040203");
        reply.append(static_cast<char>(0x30)); // our UDP port, 0x3039 = 12345
        reply.append(static_cast<char>(0x39));
        reply.append('\xe7');
        m_client->write(reply);
    }

    void handleSubscribe(const QByteArray& frame) {
        m_subscribesSeen.append(frame);

        // 14-byte handle reply: F0 00 01 <6 bytes> 00 02 <handle:2> F7
        QByteArray reply = QByteArray::fromHex("f00001000000010002");
        reply.append(static_cast<char>(0x00));
        reply.append(static_cast<char>(0x02));
        reply.append(static_cast<char>(0xAB));
        reply.append(static_cast<char>(0xC0 + m_subscribesSeen.size()));
        reply.append('\xf7');
        Q_ASSERT(reply.size() == 14);
        m_client->write(reply);
    }

    QTcpServer m_server;
    QTcpSocket* m_client = nullptr;
    QByteArray m_buffer;
    QList<QByteArray> m_subscribesSeen;
    quint16 m_clientUdpPort = 0;
    bool m_sawSeed = false;
    bool m_mute = false;
};

} // namespace

// Drives a real DLiveProtocol against a stub MixRack: the frame-builder tests
// cover bytes, these cover the session.
class TestAllenHeathSession : public QObject {
    Q_OBJECT

  private slots:
    void connect_completesHandshake() {
        StubMixRack rack;
        DLiveProtocol p(MixerCapabilities::forConsole(ConsoleType::DLive));

        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QVERIFY(p.connect("127.0.0.1", rack.port()));
        QVERIFY(connectedSpy.wait(5000));

        QVERIFY(p.isConnected());
        QCOMPARE(p.connectionState(), ConnectionState::Connected);

        // the seed must have carried our real, bound UDP port
        QVERIFY(rack.sawSeed());
        QVERIFY(rack.clientUdpPort() != 0);

        // every object the control path addresses got subscribed
        QCOMPARE(rack.subscribesSeen().size(), 6);
        QVERIFY(rack.subscribesSeen().first().contains("DR Box Identification"));
        QVERIFY(rack.subscribesSeen().at(2).contains("Input Mixer"));
        // without this one the console never starts the UDP meter stream
        QVERIFY(rack.subscribesSeen().last().contains("Metering Sources"));
    }

    void meters_reachTheChannelMonitor() {
        StubMixRack rack;
        DLiveProtocol p(MixerCapabilities::forConsole(ConsoleType::DLive));

        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QSignalSpy meterSpy(&p, &MixerProtocol::channelMeter);
        QVERIFY(p.connect("127.0.0.1", rack.port()));
        QVERIFY(connectedSpy.wait(5000));

        rack.sendMeters({{1, 0x80}, {2, 0x00}, {7, 0xFF}});
        QTRY_VERIFY_WITH_TIMEOUT(meterSpy.size() >= 128, 3000);

        // channel is the record's position, not a field on the wire
        QCOMPARE(meterSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(meterSpy.at(0).at(1).toFloat(), 0x80 / 253.0f);
        // 0 is -inf and 0xFF marks a channel the console is not metering
        QCOMPARE(meterSpy.at(1).at(1).toFloat(), 0.0f);
        QCOMPARE(meterSpy.at(6).at(1).toFloat(), 0.0f);
    }

    void sceneChangedAtTheConsole_isReported() {
        StubMixRack rack;
        DLiveProtocol p(MixerCapabilities::forConsole(ConsoleType::DLive));

        QSignalSpy connectedSpy(&p, &MixerProtocol::connected);
        QSignalSpy sceneSpy(&p, &MixerProtocol::sceneChanged);
        QVERIFY(p.connect("127.0.0.1", rack.port()));
        QVERIFY(connectedSpy.wait(5000));

        rack.sendSceneChanged(42);
        QTRY_COMPARE_WITH_TIMEOUT(sceneSpy.size(), 1, 3000);
        QCOMPARE(sceneSpy.at(0).at(0).toInt(), 42);
    }

    void subscribesWaitForSeedReply() {
        // a console that never answers the port exchange must never see a subscribe
        StubMixRack rack;
        rack.setMute(true);
        DLiveProtocol p(MixerCapabilities::forConsole(ConsoleType::DLive));

        QVERIFY(p.connect("127.0.0.1", rack.port()));
        QTest::qWait(500);

        QVERIFY(rack.sawSeed());
        QVERIFY(rack.subscribesSeen().isEmpty());
        QVERIFY(!p.isConnected());
    }

    void silentConsoleReportsErrorNotHang() {
        StubMixRack rack;
        rack.setMute(true);
        DLiveProtocol p(MixerCapabilities::forConsole(ConsoleType::DLive));

        QSignalSpy errorSpy(&p, &MixerProtocol::connectionError);
        QVERIFY(p.connect("127.0.0.1", rack.port()));

        QVERIFY(errorSpy.wait(8000));
        QCOMPARE(p.connectionState(), ConnectionState::Disconnected);
    }
};

QTEST_MAIN(TestAllenHeathSession)
#include "test_allenheath_session.moc"
