#pragma once

#include "../../core/LevelDb.h"
#include "AllenHeathMidiProtocol.h"

namespace OpenMix {

// Allen & Heath Qu series (Qu-16, Qu-24, Qu-32) protocol, MIDI over TCP 51325.
// All Qu models run the same 32-channel DSP, differing only in fader count.
//
// Qu is not SQ with different numbers: the two NRPN halves are swapped. Qu puts
// the channel in the MSB and the parameter id in the LSB (BN 63 CH, BN 62 ID,
// BN 06 VA, BN 26 VX), levels are 7-bit, and mutes are Note On/Off rather than an
// NRPN. Verified against the Qu Mixer MIDI Protocol V1.9+ (Oct 2021), which
// covers Qu-16/24/32 alongside Qu-Pac and Qu-SB.
class QuProtocol : public AllenHeathMidiProtocol {
    Q_OBJECT

  public:
    explicit QuProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath Qu MIDI/TCP Protocol"; }

    void sendParameter(const QString& path, const QVariant& value) override;

  protected:
    void initializeSnapshotParams() override;
    QString dcaFaderPath(int dca) const override;
    QString dcaMutePath(int dca) const override;

    // channel numbers, from the protocol doc's channel table
    static constexpr int CH_INPUT_BASE = 0x20; // Input 1-32   = 20..3F
    static constexpr int CH_DCA_BASE = 0x10;   // DCA Group 1-4 = 10..13

    // NRPN parameter ids
    static constexpr int ID_FADER = 0x17;    // VA = -inf..+10 dB = 00..7F
    static constexpr int ID_FADER_VX = 0x07; // the data-entry LSB the fader wants
    static constexpr int FADER_UNITY = 0x62; // 0 dB

    QByteArray buildFader(int channelId, double dB);
    static QByteArray buildMute(int channelId, bool muted);
    QByteArray buildSceneRecall(int sceneNumber) override;

    // dB -> the console's 7-bit level, through the Fader / Send Level table
    static int levelFromDb(double dB);
};

} // namespace OpenMix
