#pragma once

#include "AllenHeathMidiProtocol.h"

namespace OpenMix {

// Allen & Heath GLD series (GLD-80, GLD-112), MIDI over TCP 51325, per the GLD
// MIDI and TCP/IP Protocol V1.4. Like Qu, the channel goes in the NRPN MSB and
// mutes are Note On, but the fader NRPN carries no data-entry LSB and the level
// table is GLD's own. Names and colours are SysEx.
//
// The console's MIDI channel (Setup / Control) has to match the one used here:
// it appears in every message and cannot be read back.
class GLDProtocol : public AllenHeathMidiProtocol {
    Q_OBJECT

  public:
    explicit GLDProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath GLD MIDI/TCP Protocol"; }

    void sendParameter(const QString& path, const QVariant& value) override;
    void setChannelName(int channel, const QString& name) override;
    void setChannelColor(int channel, int color) override;

    // MIDI channel 1-16 as set on the console; held 0-based, as it goes on the wire
    void setMidiChannel(int channel1To16);
    [[nodiscard]] int midiChannel() const { return m_midiChannel + 1; }

  protected:
    void initializeSnapshotParams() override;
    QString dcaFaderPath(int dca) const override;
    QString dcaMutePath(int dca) const override;

    // channel numbers, from the protocol doc's table
    static constexpr int CH_DCA_BASE = 0x10;   // DCA 1-16   = 10..1F
    static constexpr int CH_INPUT_BASE = 0x20; // Input 1-48 = 20..4F

    // NRPN parameter ids
    static constexpr int ID_FADER = 0x17;

    static constexpr int COLOUR_OFF = 0x00;
    static constexpr int COLOUR_MAX = 0x07;

    // the console takes 8 characters and shows 5 of them on the strip
    static constexpr int MAX_NAME_LENGTH = 8;

    QByteArray buildFader(int channelId, double dB) const;
    QByteArray buildMute(int channelId, bool muted) const;
    QByteArray buildName(int channelId, const QString& name) const;
    QByteArray buildColour(int channelId, int colour) const;

    // F0 00 00 1A 50 10 <MV> <mV> <0N>, the prefix of every GLD SysEx
    QByteArray sysexHeader() const;

    // dB -> the console's 7-bit level, through its Fader level table
    static int levelFromDb(double dB);

    int m_midiChannel = 0; // 0-based; console MIDI channel 1 by default
};

} // namespace OpenMix
