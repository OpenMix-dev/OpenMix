#include "GLDProtocol.h"

namespace OpenMix {

GLDProtocol::GLDProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathTcpProtocol(caps, parent), m_midiTransport(this) {
    initializeSnapshotParams();
}

bool GLDProtocol::connect(const QString& host, int port) {
    // secondary MIDI socket for scene recall (best-effort; property socket is the
    // primary control channel)
    (void)m_midiTransport.connect(host, GLD_MIDI_PORT);
    return AllenHeathTcpProtocol::connect(host, port);
}

void GLDProtocol::disconnect() {
    m_midiTransport.disconnect();
    AllenHeathTcpProtocol::disconnect();
}

void GLDProtocol::recallScene(int sceneNumber) {
    if (sceneNumber < 1) {
        return;
    }
    // A&H MMC scene recall over the MIDI socket (51325):
    // F0 00 00 1A 50 10 01 00 <scene> 04 00 F7 (scene is 0-based on the wire)
    QByteArray msg = QByteArray::fromHex("f000001a50100100");
    msg.append(static_cast<char>((sceneNumber - 1) & 0x7F));
    msg.append('\x04');
    msg.append('\0');
    msg.append('\xf7');
    m_midiTransport.send(msg);
}

QList<QByteArray> GLDProtocol::subscribeObjects() const {
    // GLD's identity object is "iGL Surface Identification"; it addresses channels
    // and DCAs via the Input Mixer / DCA Levels and Mutes handles.
    return {QByteArrayLiteral("DR Box Identification"),
            QByteArrayLiteral("iGL Surface Identification"), kInputMixer, kDcaLevelsAndMutes};
}

QByteArray GLDProtocol::encodeDb(double dB) const {
    // GLD dB tables (reference FUN_00415780; distinct from the Avantis/dLive twin)
    static const quint8 kPosLo[10] = {0x00, 0x1B, 0x35, 0x4E, 0x68, 0x81, 0x9B, 0xB5, 0xCE, 0xE8};
    static const quint8 kNegLo[10] = {0x00, 0xE6, 0xCC, 0xB3, 0x99, 0x7F, 0x66, 0x4C, 0x33, 0x19};
    return encodeDbTables(dB, kPosLo, kNegLo);
}

void GLDProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    // GLD control covers channel level/mute and DCA mute
    for (int i = 1; i <= m_capabilities.inputChannels; ++i) {
        m_snapshotParams.append(QString("/ch/%1/fader").arg(i));
        m_snapshotParams.append(QString("/ch/%1/mute").arg(i));
    }
    for (int i = 1; i <= m_capabilities.dcaCount && i <= 8; ++i) {
        m_snapshotParams.append(QString("/dca/%1/mute").arg(i));
    }
}

} // namespace OpenMix
