#include "protocol/MixerCapabilities.h"
#include "protocol/digico/DiGiCoProtocol.h"
#include <QtEndian>
#include <QtTest/QtTest>

using namespace OpenMix;

class TestDiGiCoProtocol : public QObject {
    Q_OBJECT

  private slots:
    void frameHasTagLengthAndMarker() {
        QByteArray frame = DiGiCoProtocol::buildFrame(
            "MMIX", QByteArray::fromHex("01110108"), DiGiCoProtocol::int32Element(0));

        QCOMPARE(frame.left(4), QByteArray("MMIX"));

        // length field covers everything after it; marker 0x11 follows.
        quint32 len = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar*>(frame.constData() + 4));
        QCOMPARE(static_cast<int>(len), frame.size() - 8);
        QCOMPARE(static_cast<quint8>(frame[8]), quint8(0x11));
    }

    void stringElementCarriesNullTerminatedText() {
        QByteArray el = DiGiCoProtocol::stringElement("Mixing");
        QCOMPARE(static_cast<quint8>(el[0]), quint8(0x31));
        quint32 len = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar*>(el.constData() + 1));
        QCOMPARE(static_cast<int>(len), 7); // "Mixing" + NUL
        QCOMPARE(el.mid(5), QByteArray("Mixing\0", 7));
    }

    void faderScalesToCentidBBigEndian() {
        DiGiCoProtocol proto(MixerCapabilities::forConsole(ConsoleType::SD12));
        // +10 dB -> 1000 centidB, trailing signed int32 big-endian.
        QByteArray frame = proto.buildFaderMessage("Mixing/InputChannel00/Fader/Level", 10.0);
        QCOMPARE(frame.right(4), QByteArray::fromHex("000003e8"));
        QVERIFY(frame.contains("Mixing/InputChannel00/Fader/Level"));
    }

    void faderClampsToOffSentinel() {
        DiGiCoProtocol proto(MixerCapabilities::forConsole(ConsoleType::SD12));
        // Below -150 dB clamps to the -15000 centidB off-sentinel (0xFFFFC568).
        QByteArray frame = proto.buildFaderMessage("Mixing/InputChannel00/Fader/Level", -200.0);
        QCOMPARE(frame.right(4), QByteArray::fromHex("ffffc568"));
    }

    void muteEncodesStateByte() {
        DiGiCoProtocol proto(MixerCapabilities::forConsole(ConsoleType::SD12));
        QCOMPARE(proto.buildMuteMessage("Mixing/InputChannel00/Mute", true).right(1),
                 QByteArray::fromHex("01"));
        QCOMPARE(proto.buildMuteMessage("Mixing/InputChannel00/Mute", false).right(1),
                 QByteArray::fromHex("00"));
    }

    void subscribeMixingTargetsMixingTree() {
        DiGiCoProtocol proto(MixerCapabilities::forConsole(ConsoleType::SD12));
        QByteArray frame = proto.buildSubscribeMixing();
        QCOMPARE(frame.left(4), QByteArray("MMIX"));
        QVERIFY(frame.contains("Mixing"));
        QCOMPARE(frame.right(1), QByteArray::fromHex("80")); // subscription flag
    }

    void capabilitiesRegistered() {
        MixerCapabilities caps = MixerCapabilities::forProtocolId("sd12");
        QCOMPARE(caps.protocolId, QString("sd12"));
        QCOMPARE(caps.defaultPort, 51321);
        QCOMPARE(caps.manufacturerName(), QString("DiGiCo"));
        QVERIFY(caps.isSupported());
        QCOMPARE(caps.displayName, QString("DiGiCo SD12"));

        DiGiCoProtocol proto(caps);
        QCOMPARE(proto.capabilities().defaultPort, 51321);
    }
};

QTEST_MAIN(TestDiGiCoProtocol)
#include "test_digico_protocol.moc"
