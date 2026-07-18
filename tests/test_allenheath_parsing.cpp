#include "protocol/MixerCapabilities.h"
#include "protocol/allenheath/AvantisProtocol.h"
#include "protocol/allenheath/DLiveProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {
// exposes the protected ACE frame builders for byte-exact checks
class AvantisProbe : public AvantisProtocol {
  public:
    AvantisProbe() : AvantisProtocol(MixerCapabilities::forConsole(ConsoleType::Avantis)) {}
    using AllenHeathTcpProtocol::areaOf;
    using AllenHeathTcpProtocol::buildChannelLevel;
    using AllenHeathTcpProtocol::buildChannelMute;
    using AllenHeathTcpProtocol::buildDcaMute;
    using AllenHeathTcpProtocol::buildKeepAlive;
    using AllenHeathTcpProtocol::buildSceneRecall;
    using AllenHeathTcpProtocol::buildSessionSeed;
    using AllenHeathTcpProtocol::buildSubscribe;
    using AllenHeathTcpProtocol::encodeDb;
    using AllenHeathTcpProtocol::parseSceneChanged;
    using AllenHeathTcpProtocol::parseSessionReply;
};

class DLiveProbe : public DLiveProtocol {
  public:
    DLiveProbe() : DLiveProtocol(MixerCapabilities::forConsole(ConsoleType::DLive)) {}
    using AllenHeathTcpProtocol::buildChannelLevel;
    using AllenHeathTcpProtocol::buildChannelLevelSpill;
    using AllenHeathTcpProtocol::buildChannelMute;
    using AllenHeathTcpProtocol::buildChannelMuteSpill;
    using AllenHeathTcpProtocol::buildDcaMute;
};

const QByteArray H = QByteArray::fromHex("abcd"); // mock learned handle
} // namespace

// Byte layouts recovered from the reference AvantisDriver/DLiveDriver (ACE
// property protocol). Handles are learned at runtime; here we inject a known
// handle (AB CD) and assert the exact outgoing frame bytes.
class TestAllenHeathParsing : public QObject {
    Q_OBJECT

  private slots:
    void encodeDb_matchesReferenceTables() {
        // exact per-0.1 dB lookup tables (reference FUN_00415330), not a formula
        AvantisProbe p;
        QCOMPARE(p.encodeDb(0.0), QByteArray::fromHex("8000"));
        QCOMPARE(p.encodeDb(10.0), QByteArray::fromHex("8a00"));
        QCOMPARE(p.encodeDb(-10.0), QByteArray::fromHex("7600"));
        QCOMPARE(p.encodeDb(3.0), QByteArray::fromHex("8300"));
        QCOMPARE(p.encodeDb(-96.0), QByteArray::fromHex("0000")); // -inf
        QCOMPARE(p.encodeDb(5.3), QByteArray::fromHex("854f"));
        QCOMPARE(p.encodeDb(-5.3), QByteArray::fromHex("7aa6")); // floor -6, .3 neg
        QCOMPARE(p.encodeDb(-0.5), QByteArray::fromHex("7f73"));
        QCOMPARE(p.encodeDb(-6.0), QByteArray::fromHex("7a00"));
    }

    void sessionSeed_advertisesUdpPort() {
        // the 01 is what makes it a client seed; sending the console's 02 reply
        // back at it opens no session
        AvantisProbe p;
        QCOMPARE(p.buildSessionSeed(0x1234), QByteArray::fromHex("e0000401031234e7"));
        QCOMPARE(p.buildSessionSeed(51321), QByteArray::fromHex("e000040103c879e7"));
        QCOMPARE(p.buildSessionSeed(0), QByteArray::fromHex("e0000401030000e7"));
    }

    void sessionReply_yieldsConsoleUdpPort() {
        AvantisProbe p;
        QCOMPARE(p.parseSessionReply(QByteArray::fromHex("e0000402031234e7")), quint16(0x1234));
        // the client's own seed is not a reply, however similar it looks
        QCOMPARE(p.parseSessionReply(QByteArray::fromHex("e0000401031234e7")), quint16(0));
        QCOMPARE(p.parseSessionReply(QByteArray::fromHex("e00003425945e7")), quint16(0)); // BYE
        QCOMPARE(p.parseSessionReply(QByteArray()), quint16(0));
        QCOMPARE(p.parseSessionReply(QByteArray::fromHex("e00004020312")), quint16(0)); // truncated
    }

    void keepAlive_matchesReference() {
        AvantisProbe p;
        QCOMPARE(p.buildKeepAlive(), QByteArray::fromHex("e0000103e7"));
    }

    void sceneChanged_isZeroBasedOnTheWire() {
        AvantisProbe p;
        // F0 <class> <area 000B> <src> <op> <len> | <scene-1 BE>
        QCOMPARE(p.parseSceneChanged(QByteArray::fromHex("f00000000b000100020002"
                                                         "0000")),
                 1);
        QCOMPARE(p.parseSceneChanged(QByteArray::fromHex("f00000000b000100020002"
                                                         "0063")),
                 100);
        QCOMPARE(p.parseSceneChanged(QByteArray::fromHex("f00000000b000100020002"
                                                         "0100")),
                 257);
        // a frame from any other functional area is not a scene change
        QCOMPARE(p.parseSceneChanged(QByteArray::fromHex("f000000001000100020002"
                                                         "0000")),
                 0);
        QCOMPARE(p.parseSceneChanged(QByteArray::fromHex("f00000000b")), 0);
    }

    void areaOf_readsTheFunctionalArea() {
        AvantisProbe p;
        QCOMPARE(p.areaOf(QByteArray::fromHex("f000000006000100020002")), quint16(0x0006));
        QCOMPARE(p.areaOf(QByteArray::fromHex("f0000000010001")), quint16(0x0001));
        QCOMPARE(p.areaOf(QByteArray::fromHex("f000")), quint16(0));
    }

    void avantis_firmwareGatedOp() {
        AvantisProbe p;
        p.setFirmwareRevision(96500); // <= 96884 -> base op 0x25
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0001250000028000f7"));
        p.setFirmwareRevision(97000); // > 96884 -> extended op 0x2B
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd00012b0000028000f7"));
    }

    void dlive_extendedOpForNon1x() {
        DLiveProbe p;
        p.setFirmwareVersion("1.90"); // 1.x -> base op 0x30
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0001300000028000f7"));
        p.setFirmwareVersion("2.00"); // non-1.x -> extended op 0x38
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0001380000028000f7"));
    }

    void dlive_spillFrames() {
        DLiveProbe p;
        // op 0x12, level idx ((ch-1)&7)+0x80, mute idx +0x98, bank handle
        QCOMPARE(p.buildChannelLevelSpill(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0001128000028000f7"));
        QCOMPARE(p.buildChannelMuteSpill(H, 1, true),
                 QByteArray::fromHex("f00000abcd00011298000101f7"));
    }

    void subscribe_matchesReferenceBlob() {
        // "Input Mixer" subscribe is a verbatim reference blob
        QCOMPARE(AvantisProbe::buildSubscribe("Input Mixer"),
                 QByteArray::fromHex("f00001000000010004000c496e707574204d6978657200f7"));
    }

    void avantis_channelLevel() {
        AvantisProbe p;
        // F0 00 00 <H> 00 00 25 (ch-1) 00 02 <dB> F7 ; ch1 @ 0 dB
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0001250000028000f7"));
        // ch2 @ +10 dB -> idx 01, value 8A00
        QCOMPARE(p.buildChannelLevel(H, 2, 10.0),
                 QByteArray::fromHex("f00000abcd0001250100028a00f7"));
    }

    void avantis_channelMute() {
        AvantisProbe p;
        // op 0x26, idx = (ch-1)+0x20, disc 01 ; ch1 mute on / off
        QCOMPARE(p.buildChannelMute(H, 1, true), QByteArray::fromHex("f00000abcd00012620000101f7"));
        QCOMPARE(p.buildChannelMute(H, 1, false),
                 QByteArray::fromHex("f00000abcd00012620000100f7"));
    }

    void avantis_dcaMute() {
        AvantisProbe p;
        // op 0x10, idx = (dca-1)+0x18 ; dca1 on, dca2 -> idx 0x19
        QCOMPARE(p.buildDcaMute(H, 1, true), QByteArray::fromHex("f00000abcd00011018000101f7"));
        QCOMPARE(p.buildDcaMute(H, 2, true), QByteArray::fromHex("f00000abcd00011019000101f7"));
    }

    void sceneRecall_matchesReference() {
        AvantisProbe p;
        // F0 00 00 <H> 00 00 10 00 00 02 <(scene-1) BE> F7 ; scene 1 -> 0000
        QCOMPARE(p.buildSceneRecall(H, 1), QByteArray::fromHex("f00000abcd0001100000020000f7"));
        // scene 256 -> (255) = 00FF
        QCOMPARE(p.buildSceneRecall(H, 256), QByteArray::fromHex("f00000abcd00011000000200fff7"));
    }

    void dlive_opcodesDifferFromAvantis() {
        DLiveProbe p;
        // dLive channel level op 0x30, mute op 0x31 / plane 0x80, DCA plane 0x20
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0001300000028000f7"));
        QCOMPARE(p.buildChannelMute(H, 1, true),
                 QByteArray::fromHex("f00000abcd00013180000101f7")); // idx (0)+0x80
        QCOMPARE(p.buildDcaMute(H, 1, true),
                 QByteArray::fromHex("f00000abcd00011020000101f7")); // idx (0)+0x20
    }

    void emptyHandle_producesNoFrame() {
        AvantisProbe p;
        QVERIFY(p.buildChannelLevel(QByteArray(), 1, 0.0).isEmpty());
        QVERIFY(p.buildDcaMute(QByteArray(), 1, true).isEmpty());
    }
};

QTEST_MAIN(TestAllenHeathParsing)
#include "test_allenheath_parsing.moc"
