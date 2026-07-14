#include "protocol/MixerCapabilities.h"
#include "protocol/allenheath/AllenHeathMidiProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {
// exposes the protected NRPN builder + satisfies the pure virtuals
class SqProbe : public AllenHeathMidiProtocol {
  public:
    using AllenHeathMidiProtocol::AllenHeathMidiProtocol;
    using AllenHeathMidiProtocol::buildNRPNMessage;
    using AllenHeathMidiProtocol::buildSceneRecall;
    static constexpr int dcaLevelMsb() { return DCA_LEVEL_MSB; }
    static constexpr int dcaLevelLsbBase() { return DCA_LEVEL_LSB_BASE; }
    static constexpr int dcaMuteMsb() { return DCA_MUTE_MSB; }

  protected:
    void initializeSnapshotParams() override {}
    QString dcaFaderPath(int dca) const override { return QString("/dca/%1/fader").arg(dca); }
    QString dcaMutePath(int dca) const override { return QString("/dca/%1/mute").arg(dca); }
};
} // namespace

// Verifies the SQ NRPN encoding against the byte examples printed in the official
// Allen & Heath SQ MIDI Protocol Issue 5 (p11), and the verified parameter
// numbers (Inputs-to-LR level p22).
class TestAllenHeathMidi : public QObject {
    Q_OBJECT

    SqProbe* p = nullptr;

  private slots:
    void init() { p = new SqProbe(MixerCapabilities::forConsole(ConsoleType::Loopback)); }
    void cleanup() {
        delete p;
        p = nullptr;
    }

    void nrpn_matchesOfficialMuteExamples() {
        // p11: "Ip1, Mute On, Ch1" = B0 63 00 B0 62 00 B0 06 00 B0 26 01
        QCOMPARE(p->buildNRPNMessage(0, 0x00, 0x00, 0x00, 0x01),
                 QByteArray::fromHex("B06300B06200B00600B02601"));
        // p11: "LR mix, Mute Off, Ch1" = B0 63 00 B0 62 44 B0 06 00 B0 26 00
        QCOMPARE(p->buildNRPNMessage(0, 0x00, 0x44, 0x00, 0x00),
                 QByteArray::fromHex("B06300B06244B00600B02600"));
    }

    void channelFader_usesVerifiedLrLevelParam() {
        // input ch1 -> LR level at unity: MSB 0x40, LSB 0x00, 14-bit 16383 (VC/VF 7F/7F)
        QCOMPARE(p->buildNRPNMessage(0, 0x40, 0x00, 0x7F, 0x7F),
                 QByteArray::fromHex("B06340B06200B0067FB0267F"));
        // ch5 -> LR uses LSB = 4 (N-1)
        QCOMPARE(p->buildNRPNMessage(0, 0x40, 0x04, 0x40, 0x00).left(6),
                 QByteArray::fromHex("B06340B06204"));
    }

    void dcaFader_usesVerifiedLevelParam() {
        // A&H SQ MIDI Protocol Level table: DCA N level = MSB 0x4F, LSB 0x67 + N-1
        QCOMPARE(SqProbe::dcaLevelMsb(), 0x4F);
        QCOMPARE(SqProbe::dcaLevelLsbBase(), 0x67);
        // DCA1 fader at unity: MSB 0x4F, LSB 0x67
        QCOMPARE(
            p->buildNRPNMessage(0, SqProbe::dcaLevelMsb(), SqProbe::dcaLevelLsbBase(), 0x7F, 0x7F)
                .left(6),
            QByteArray::fromHex("B0634FB06267"));
        // DCA8 -> LSB 0x6E
        QCOMPARE(
            p->buildNRPNMessage(0, SqProbe::dcaLevelMsb(), SqProbe::dcaLevelLsbBase() + 7, 0, 0)
                .left(6),
            QByteArray::fromHex("B0634FB0626E"));
    }

    void dcaMute_usesVerifiedParam() {
        // DCA N mute = MSB 0x02, LSB N-1
        QCOMPARE(SqProbe::dcaMuteMsb(), 0x02);
    }

    void sceneRecall_matchesOfficialBankProgram() {
        // A&H SQ MIDI Protocol scene recall = Bank Select (CC0) + Program Change,
        // scene is 1-based, MIDI offset -1, banks of 128. Doc examples (Ch1):
        QCOMPARE(p->buildSceneRecall(7), QByteArray::fromHex("B00000C006"));   // Scene 7
        QCOMPARE(p->buildSceneRecall(120), QByteArray::fromHex("B00000C077")); // Scene 120
        QCOMPARE(p->buildSceneRecall(156), QByteArray::fromHex("B00001C01B")); // Scene 156
        QCOMPARE(p->buildSceneRecall(264), QByteArray::fromHex("B00002C007")); // Scene 264
    }

    void sceneRecall_noneIsEmpty() {
        QVERIFY(p->buildSceneRecall(0).isEmpty());  // 0 -> index -1 -> unset
        QVERIFY(p->buildSceneRecall(-1).isEmpty()); // explicit None
    }
};

QTEST_MAIN(TestAllenHeathMidi)
#include "test_allenheath_midi.moc"
