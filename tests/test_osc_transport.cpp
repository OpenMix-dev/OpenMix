#include "protocol/transport/OscTransport.h"
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
}

class TestOscTransport : public QObject {
    Q_OBJECT

  private slots:
    void replyToRequestSourcePortIsReceived() {
        QUdpSocket mixer;
        QVERIFY(mixer.bind(QHostAddress::LocalHost, 0));
        const quint16 mixerPort = mixer.localPort();

        QObject::connect(&mixer, &QUdpSocket::readyRead, [&mixer]() {
            while (mixer.hasPendingDatagrams()) {
                const QNetworkDatagram in = mixer.receiveDatagram();
                lo_message msg = lo_message_new();
                lo_message_add_string(msg, "X32");
                const QByteArray reply = oscBytes("/xinfo", msg);
                lo_message_free(msg);
                mixer.writeDatagram(reply, in.senderAddress(), in.senderPort());
            }
        });

        OscTransport transport;
        QSignalSpy rx(&transport, &OscTransport::messageReceived);
        QVERIFY(transport.connect("127.0.0.1", mixerPort));
        transport.send("/xinfo");

        QVERIFY(rx.wait(2000));
        QCOMPARE(rx.count(), 1);
        QCOMPARE(rx.at(0).at(0).toString(), QStringLiteral("/xinfo"));
        QCOMPARE(rx.at(0).at(1).toString(), QStringLiteral("X32"));
    }

    void sendEmitsWellFormedOsc() {
        QUdpSocket mixer;
        QVERIFY(mixer.bind(QHostAddress::LocalHost, 0));
        const quint16 mixerPort = mixer.localPort();
        QSignalSpy wire(&mixer, &QUdpSocket::readyRead);

        OscTransport transport;
        QVERIFY(transport.connect("127.0.0.1", mixerPort));
        transport.send("/ch/01/mix/on", 1);

        QVERIFY(wire.wait(2000));
        const QNetworkDatagram dg = mixer.receiveDatagram();
        const QByteArray data = dg.data();

        const int typeStart = data.indexOf('\0');
        QCOMPARE(data.left(typeStart), QByteArray("/ch/01/mix/on"));
        QCOMPARE(data.size() % 4, 0);

        const int tagAt = ((typeStart + 4) / 4) * 4;
        QCOMPARE(data.mid(tagAt, 2), QByteArray(",i"));

        const int argAt = data.size() - 4;
        const quint32 arg = (quint8(data[argAt]) << 24) | (quint8(data[argAt + 1]) << 16) |
                            (quint8(data[argAt + 2]) << 8) | quint8(data[argAt + 3]);
        QCOMPARE(arg, 1u);
    }
};

QTEST_MAIN(TestOscTransport)
#include "test_osc_transport.moc"
