#include "core/Cue.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/allenheath/AllenHeathMidiProtocol.h"
#include "protocol/allenheath/GLDProtocol.h"
#include "protocol/allenheath/QuProtocol.h"
#include <QtTest/QtTest>

using namespace OpenMix;

namespace {
// exposes Qu's builders; Qu's map is unrelated to SQ's
class QuProbe : public QuProtocol {
  public:
    QuProbe() : QuProtocol(MixerCapabilities::forConsole(ConsoleType::Qu16)) {}
    using QuProtocol::buildFader;
    using QuProtocol::buildMute;
    using QuProtocol::buildSceneRecall;
    using QuProtocol::levelFromDb;
};

// exposes GLD's builders; GLD's map is its own again
class GldProbe : public GLDProtocol {
  public:
    GldProbe() : GLDProtocol(MixerCapabilities::forConsole(ConsoleType::GLD80)) {}
    using GLDProtocol::buildColour;
    using GLDProtocol::buildFader;
    using GLDProtocol::buildMute;
    using GLDProtocol::buildName;
    using GLDProtocol::levelFromDb;
    using GLDProtocol::setMidiChannel;
};

// exposes the protected NRPN builder + satisfies the pure virtuals
class SqProbe : public AllenHeathMidiProtocol {
  public:
    using AllenHeathMidiProtocol::AllenHeathMidiProtocol;
    using AllenHeathMidiProtocol::buildNRPNMessage;
    using AllenHeathMidiProtocol::buildSceneRecall;
    using AllenHeathMidiProtocol::encodeAudioTaper;
    using AllenHeathMidiProtocol::encodeLevel14;
    using AllenHeathMidiProtocol::encodeLinearTaper;
    using AllenHeathMidiProtocol::FaderLaw;
    using AllenHeathMidiProtocol::setFaderLaw;
    static constexpr double negInfDb() { return NEG_INF_DB; }
    static constexpr int dcaLevelMsb() { return DCA_LEVEL_MSB; }
    static constexpr int dcaLevelLsbBase() { return DCA_LEVEL_LSB_BASE; }
    static constexpr int dcaMuteMsb() { return DCA_MUTE_MSB; }

  protected:
    void initializeSnapshotParams() override {}
    QString dcaFaderPath(int dca) const override { return QString("/dca/%1/fader").arg(dca); }
    QString dcaMutePath(int dca) const override { return QString("/dca/%1/mute").arg(dca); }
};
} // namespace

// Byte examples come from the official protocol docs: SQ MIDI Protocol Issue 5
// and Qu Mixer MIDI Protocol V1.9+. The two families share a transport and
// nothing else, so each has its own expectations here.
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
        // SQ Iss5 p24 / Qu Iss2 p25 Mix Sends "Control": DCA1 = 4F 20 .. DCA8 = 4F 27
        QCOMPARE(SqProbe::dcaLevelMsb(), 0x4F);
        QCOMPARE(SqProbe::dcaLevelLsbBase(), 0x20);
        QCOMPARE(
            p->buildNRPNMessage(0, SqProbe::dcaLevelMsb(), SqProbe::dcaLevelLsbBase(), 0x7F, 0x7F)
                .left(6),
            QByteArray::fromHex("B0634FB06220"));
        QCOMPARE(
            p->buildNRPNMessage(0, SqProbe::dcaLevelMsb(), SqProbe::dcaLevelLsbBase() + 7, 0, 0)
                .left(6),
            QByteArray::fromHex("B0634FB06227"));
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

    // --- fader laws (SQ Iss5 p20) ---

    void linearTaper_reproducesThePrintedTable() {
        // every anchor the doc prints, as <dB, VC, VF>. The doc rounds its own
        // values, so allow the 2-LSB slack that rounding introduces.
        struct Row {
            double dB;
            quint8 vc;
            quint8 vf;
        };
        static const Row rows[] = {
            {-89, 0x24, 0x16}, {-85, 0x27, 0x71}, {-80, 0x2C, 0x42}, {-75, 0x31, 0x14},
            {-70, 0x35, 0x65}, {-65, 0x3A, 0x37}, {-60, 0x3F, 0x09}, {-55, 0x43, 0x5A},
            {-50, 0x48, 0x2C}, {-45, 0x4C, 0x7D}, {-40, 0x51, 0x4F}, {-35, 0x56, 0x21},
            {-30, 0x5A, 0x72}, {-25, 0x5F, 0x44}, {-20, 0x64, 0x16}, {-15, 0x68, 0x67},
            {-10, 0x6D, 0x39}, {-5, 0x72, 0x0A},  {0, 0x76, 0x5C},   {5, 0x7B, 0x2E},
            {10, 0x7F, 0x7F},
        };
        for (const Row& r : rows) {
            const int expected = (r.vc << 7) | r.vf;
            const int actual = SqProbe::encodeLinearTaper(r.dB);
            QVERIFY2(
                std::abs(actual - expected) <= 2,
                qPrintable(
                    QString("%1 dB: got %2, doc says %3").arg(r.dB).arg(actual).arg(expected)));
        }
        QCOMPARE(SqProbe::encodeLinearTaper(SqProbe::negInfDb()), quint16(0));
    }

    void audioTaper_reproducesThePrintedTable() {
        // the audio taper is a curve, so its anchors must come back exactly
        struct Row {
            double dB;
            quint8 vc;
            quint8 vf;
        };
        static const Row rows[] = {
            {-45, 0x0C, 0x00}, {-40, 0x0F, 0x40}, {-30, 0x1F, 0x00}, {-20, 0x2E, 0x40},
            {-10, 0x3E, 0x00}, {-5, 0x4E, 0x40},  {-1, 0x5E, 0x00},  {0, 0x62, 0x00},
            {5, 0x73, 0x40},   {10, 0x7F, 0x40},
        };
        for (const Row& r : rows) {
            QCOMPARE(SqProbe::encodeAudioTaper(r.dB), quint16((r.vc << 7) | r.vf));
        }
        QCOMPARE(SqProbe::encodeAudioTaper(SqProbe::negInfDb()), quint16(0));
    }

    void faderLaw_reachesTheWire() {
        // the setting exists so an operator can match the desk; prove it changes
        // the bytes a fader move puts on the wire, not just an internal flag
        SqProbe probe(MixerCapabilities::forConsole(ConsoleType::Loopback));
        probe.setFaderLaw(SqProbe::FaderLaw::LinearTaper);
        const quint16 lin = probe.encodeLevel14(0.0);
        probe.setFaderLaw(SqProbe::FaderLaw::AudioTaper);
        const quint16 aud = probe.encodeLevel14(0.0);
        QVERIFY(lin != aud);
        QCOMPARE(lin, quint16((0x76 << 7) | 0x5C)); // doc p20 linear 0 dB
        QCOMPARE(aud, quint16(0x62 << 7));          // doc p20 audio 0 dB
    }

    void faderLaw_selectsTheCurve() {
        SqProbe probe(MixerCapabilities::forConsole(ConsoleType::Loopback));
        // linear is the console's standard mode, so it is the default
        QCOMPARE(probe.faderLaw(), SqProbe::FaderLaw::LinearTaper);
        QCOMPARE(probe.encodeLevel14(0.0), quint16((0x76 << 7) | 0x5C));
        probe.setFaderLaw(SqProbe::FaderLaw::AudioTaper);
        QCOMPARE(probe.encodeLevel14(0.0), quint16(0x62 << 7));
    }

    // --- Qu, per the Qu Mixer MIDI Protocol V1.9+ ---

    void qu_faderPutsTheChannelInTheMsb() {
        // BN 63 CH | BN 62 17 | BN 06 VA | BN 26 07 - the opposite of SQ, which
        // puts the parameter in the MSB and the channel in the LSB
        QuProbe q;
        // Input 1 = CH 20 at unity: 0 dB = VA 62
        QCOMPARE(q.buildFader(0x20, 0.0), QByteArray::fromHex("B06320B06217B00662B02607"));
        // Input 32 = CH 3F, fully down (-inf) / fully up (+10 dB)
        QCOMPARE(q.buildFader(0x3F, Cue::NEG_INF_DB),
                 QByteArray::fromHex("B0633FB06217B00600B02607"));
        QCOMPARE(q.buildFader(0x3F, 10.0), QByteArray::fromHex("B0633FB06217B0067FB02607"));
        // DCA 1 = CH 10
        QCOMPARE(q.buildFader(0x10, 10.0), QByteArray::fromHex("B06310B06217B0067FB02607"));
    }

    void qu_levelReproducesThePrintedTable() {
        // Fader / Send Level table: every dB anchor the doc prints, exactly
        struct Row {
            double dB;
            quint8 va;
        };
        static const Row rows[] = {
            {10, 0x7F},  {5, 0x72},   {0, 0x62},   {-5, 0x4F},  {-10, 0x3F}, {-15, 0x36},
            {-20, 0x2F}, {-25, 0x27}, {-30, 0x1F}, {-35, 0x17}, {-40, 0x10}, {-45, 0x0C},
        };
        for (const Row& r : rows) {
            QCOMPARE(QuProbe::levelFromDb(r.dB), static_cast<int>(r.va));
        }
        QCOMPARE(QuProbe::levelFromDb(SqProbe::negInfDb()), 0x00);
        // beyond the printed ends the console still clamps to the table
        QCOMPARE(QuProbe::levelFromDb(20.0), 0x7F);
        QCOMPARE(QuProbe::levelFromDb(-50.0), 0x0C);
    }

    void qu_faderTakesDbDirectly() {
        QuProbe q;
        QCOMPARE(q.buildFader(0x20, 0.0), QByteArray::fromHex("B06320B06217B00662B02607"));
        QCOMPARE(q.buildFader(0x20, Cue::NEG_INF_DB),
                 QByteArray::fromHex("B06320B06217B00600B02607"));
        QCOMPARE(q.buildFader(0x20, 10.0), QByteArray::fromHex("B06320B06217B0067FB02607"));
    }

    void qu_muteIsANoteNotAnNrpn() {
        QuProbe q;
        // mute on = velocity 7F, off = 3F; each followed by a note off
        QCOMPARE(q.buildMute(0x20, true), QByteArray::fromHex("90207F902000"));
        QCOMPARE(q.buildMute(0x20, false), QByteArray::fromHex("90203F902000"));
        QCOMPARE(q.buildMute(0x13, true), QByteArray::fromHex("90137F901300"));
    }

    void qu_sceneRecallSelectsBankOne() {
        // B0 00 00, B0 20 00, C0 SS with scenes 1..100 = 00..63
        QuProbe q;
        QCOMPARE(q.buildSceneRecall(1), QByteArray::fromHex("B00000B02000C000"));
        QCOMPARE(q.buildSceneRecall(100), QByteArray::fromHex("B00000B02000C063"));
        QVERIFY(q.buildSceneRecall(0).isEmpty());
    }

    void qu_capabilitiesMatchTheDoc() {
        const auto caps = MixerCapabilities::forConsole(ConsoleType::Qu32);
        QCOMPARE(caps.defaultPort, 51325);
        QCOMPARE(caps.dcaCount, 4); // DCA Groups 1 to 4
        QCOMPARE(caps.scenes, 100); // Scene 1 to 100
        QCOMPARE(caps.inputChannels, 32);
    }

    // --- GLD, per the GLD MIDI and TCP/IP Protocol V1.4 ---

    void gld_levelReproducesThePrintedTable() {
        // its own table: 0 dB is 6B here, 62 on Qu
        struct Row {
            double dB;
            quint8 lv;
        };
        static const Row rows[] = {
            {10, 0x7F},  {5, 0x74},   {0, 0x6B},   {-5, 0x61},  {-10, 0x57}, {-15, 0x4D},
            {-20, 0x43}, {-25, 0x39}, {-30, 0x2F}, {-35, 0x25}, {-40, 0x1B}, {-45, 0x11},
        };
        for (const Row& r : rows) {
            QCOMPARE(GldProbe::levelFromDb(r.dB), static_cast<int>(r.lv));
        }
        QCOMPARE(GldProbe::levelFromDb(NEG_INF_DB), 0x00);
    }

    void gld_faderCarriesNoDataEntryLsb() {
        // BN 63 CH | BN 62 17 | BN 06 LV - three messages, where SQ and Qu send four
        GldProbe g;
        // Input 1 = CH 20 at unity
        QCOMPARE(g.buildFader(0x20, 0.0), QByteArray::fromHex("B06320B06217B0066B"));
        // DCA 1 = CH 10, fully down
        QCOMPARE(g.buildFader(0x10, NEG_INF_DB), QByteArray::fromHex("B06310B06217B00600"));
    }

    void gld_muteIsANote() {
        GldProbe g;
        QCOMPARE(g.buildMute(0x20, true), QByteArray::fromHex("90207F902000"));
        QCOMPARE(g.buildMute(0x20, false), QByteArray::fromHex("90203F902000"));
    }

    void gld_nameAndColourAreSysex() {
        // header F0 00 00 1A 50 10 01 00 0N, then 03 CH <ascii> F7 / 06 CH Col F7
        GldProbe g;
        QCOMPARE(g.buildName(0x20, "Vox"), QByteArray::fromHex("f000001a50100100000320566f78f7"));
        QCOMPARE(g.buildColour(0x20, 0x02), QByteArray::fromHex("f000001a5010010000062002f7"));
    }

    void gld_midiChannelReachesEveryMessage() {
        // the channel rides the status byte and the SysEx header, and the console
        // cannot report it: a mismatch silently talks to nobody
        GldProbe g;
        g.setMidiChannel(3); // console channel 3 -> N = 2
        QCOMPARE(g.midiChannel(), 3);
        QCOMPARE(g.buildFader(0x20, 0.0), QByteArray::fromHex("B26320B26217B2066B"));
        QCOMPARE(g.buildMute(0x20, true), QByteArray::fromHex("92207F922000"));
        QCOMPARE(g.buildColour(0x20, 0x02), QByteArray::fromHex("f000001a5010010002062002f7"));

        g.setMidiChannel(1); // the default: N = 0
        QCOMPARE(g.buildFader(0x20, 0.0), QByteArray::fromHex("B06320B06217B0066B"));
    }

    void gld_capabilitiesMatchTheDoc() {
        const auto caps = MixerCapabilities::forConsole(ConsoleType::GLD80);
        QCOMPARE(caps.defaultPort, 51325); // MIDI over TCP, not ACE 51321
        QCOMPARE(caps.protocol, ProtocolType::MidiTcp);
        QCOMPARE(caps.dcaCount, 16); // DCA 1 to 16 = 10..1F
        QCOMPARE(caps.scenes, 500);  // 500 scenes in 4 banks
    }
};

QTEST_MAIN(TestAllenHeathMidi)
#include "test_allenheath_midi.moc"
