#include "protocol/MixerCapabilities.h"
#include "protocol/allenheath/AvantisProtocol.h"
#include "protocol/allenheath/DLiveProtocol.h"
#include "protocol/allenheath/GLDProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {
// exposes the protected ACE frame builders for byte-exact checks
class AvantisProbe : public AvantisProtocol {
  public:
    AvantisProbe() : AvantisProtocol(MixerCapabilities::forConsole(ConsoleType::Avantis)) {}
    using AllenHeathTcpProtocol::buildChannelLevel;
    using AllenHeathTcpProtocol::buildChannelMute;
    using AllenHeathTcpProtocol::buildDcaMute;
    using AllenHeathTcpProtocol::buildSceneRecall;
    using AllenHeathTcpProtocol::buildSubscribe;
    using AllenHeathTcpProtocol::encodeDb;
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

class GLDProbe : public GLDProtocol {
  public:
    GLDProbe() : GLDProtocol(MixerCapabilities::forConsole(ConsoleType::GLD80)) {}
    using AllenHeathTcpProtocol::buildChannelLevel;
    using AllenHeathTcpProtocol::buildChannelMute;
    using AllenHeathTcpProtocol::buildDcaMute;
    using AllenHeathTcpProtocol::encodeDb;
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

    void avantis_firmwareGatedOp() {
        AvantisProbe p;
        p.setFirmwareRevision(96500); // <= 96884 -> base op 0x25
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0000250000028000f7"));
        p.setFirmwareRevision(97000); // > 96884 -> extended op 0x2B
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd00002b0000028000f7"));
    }

    void dlive_extendedOpForNon1x() {
        DLiveProbe p;
        p.setFirmwareVersion("1.90"); // 1.x -> base op 0x30
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0000300000028000f7"));
        p.setFirmwareVersion("2.00"); // non-1.x -> extended op 0x38
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0000380000028000f7"));
    }

    void dlive_spillFrames() {
        DLiveProbe p;
        // op 0x12, level idx ((ch-1)&7)+0x80, mute idx +0x98, bank handle
        QCOMPARE(p.buildChannelLevelSpill(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0000128000028000f7"));
        QCOMPARE(p.buildChannelMuteSpill(H, 1, true),
                 QByteArray::fromHex("f00000abcd00001298000101f7"));
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
                 QByteArray::fromHex("f00000abcd0000250000028000f7"));
        // ch2 @ +10 dB -> idx 01, value 8A00
        QCOMPARE(p.buildChannelLevel(H, 2, 10.0),
                 QByteArray::fromHex("f00000abcd0000250100028a00f7"));
    }

    void avantis_channelMute() {
        AvantisProbe p;
        // op 0x26, idx = (ch-1)+0x20, disc 01 ; ch1 mute on / off
        QCOMPARE(p.buildChannelMute(H, 1, true), QByteArray::fromHex("f00000abcd00002620000101f7"));
        QCOMPARE(p.buildChannelMute(H, 1, false),
                 QByteArray::fromHex("f00000abcd00002620000100f7"));
    }

    void avantis_dcaMute() {
        AvantisProbe p;
        // op 0x10, idx = (dca-1)+0x18 ; dca1 on, dca2 -> idx 0x19
        QCOMPARE(p.buildDcaMute(H, 1, true), QByteArray::fromHex("f00000abcd00001018000101f7"));
        QCOMPARE(p.buildDcaMute(H, 2, true), QByteArray::fromHex("f00000abcd00001019000101f7"));
    }

    void sceneRecall_matchesReference() {
        AvantisProbe p;
        // F0 00 00 <H> 00 00 10 00 00 02 <(scene-1) BE> F7 ; scene 1 -> 0000
        QCOMPARE(p.buildSceneRecall(H, 1), QByteArray::fromHex("f00000abcd0000100000020000f7"));
        // scene 256 -> (255) = 00FF
        QCOMPARE(p.buildSceneRecall(H, 256), QByteArray::fromHex("f00000abcd00001000000200fff7"));
    }

    void dlive_opcodesDifferFromAvantis() {
        DLiveProbe p;
        // dLive channel level op 0x30, mute op 0x31 / plane 0x80, DCA plane 0x20
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0000300000028000f7"));
        QCOMPARE(p.buildChannelMute(H, 1, true),
                 QByteArray::fromHex("f00000abcd00003180000101f7")); // idx (0)+0x80
        QCOMPARE(p.buildDcaMute(H, 1, true),
                 QByteArray::fromHex("f00000abcd00001020000101f7")); // idx (0)+0x20
    }

    void emptyHandle_producesNoFrame() {
        AvantisProbe p;
        QVERIFY(p.buildChannelLevel(QByteArray(), 1, 0.0).isEmpty());
        QVERIFY(p.buildDcaMute(QByteArray(), 1, true).isEmpty());
    }

    void gld_propertyFrames() {
        GLDProbe p;
        // GLD level + mute share op 0x16 (mute plane 0x90); DCA mute op 0x10 plane 0x10
        QCOMPARE(p.buildChannelLevel(H, 1, 0.0),
                 QByteArray::fromHex("f00000abcd0000160000028000f7"));
        QCOMPARE(p.buildChannelMute(H, 1, true), QByteArray::fromHex("f00000abcd00001690000101f7"));
        QCOMPARE(p.buildDcaMute(H, 1, true), QByteArray::fromHex("f00000abcd00001010000101f7"));
    }

    void gld_dbTableDiffersFromAce() {
        GLDProbe g;
        AvantisProbe a;
        // GLD uses reference FUN_00415780: positive .3 = 0x4E (ACE 0x4F)
        QCOMPARE(g.encodeDb(5.3), QByteArray::fromHex("854e"));
        QCOMPARE(a.encodeDb(5.3), QByteArray::fromHex("854f"));
        // GLD negative .3 = 0xB3
        QCOMPARE(g.encodeDb(-5.3), QByteArray::fromHex("7ab3"));
    }
};

QTEST_MAIN(TestAllenHeathParsing)
#include "test_allenheath_parsing.moc"
