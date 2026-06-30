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
};

QTEST_MAIN(TestAllenHeathMidi)
#include "test_allenheath_midi.moc"
