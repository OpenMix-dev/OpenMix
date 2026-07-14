#pragma once

#include "../transport/TcpTransport.h"
#include "AllenHeathTcpProtocol.h"

namespace OpenMix {

// Allen & Heath GLD series (GLD-80, GLD-112).
// GLD is driven over the A&H "property protocol" (the same family as
// Avantis/dLive) on TCP 51321, with its own opcodes and dB table. Scene recall
// by number is issued as an A&H MMC SysEx over a secondary MIDI socket (51325),
// matching the reference GLDDriver's two-socket design.
class GLDProtocol : public AllenHeathTcpProtocol {
    Q_OBJECT

  public:
    explicit GLDProtocol(const MixerCapabilities& caps, QObject* parent = nullptr);

    QString protocolDescription() const override { return "Allen & Heath GLD ACE Protocol"; }

    [[nodiscard]] bool connect(const QString& host, int port) override;
    void disconnect() override;
    void recallScene(int sceneNumber) override;

  protected:
    void initializeSnapshotParams() override;
    QList<QByteArray> subscribeObjects() const override;
    QByteArray encodeDb(double dB) const override;

    // GLD property opcodes recovered from GLDDriver: channel level and mute share
    // op 0x16 (mute lives at index plane 0x90); DCA mute is op 0x10 plane 0x10.
    quint8 channelLevelOp() const override { return 0x16; }
    quint8 channelMuteOp() const override { return 0x16; }
    int channelMutePlane() const override { return 0x90; }
    int dcaMutePlane() const override { return 0x10; }

  private:
    TcpTransport m_midiTransport; // secondary MIDI socket for scene MMC
    static constexpr int GLD_MIDI_PORT = 51325;
};

} // namespace OpenMix
