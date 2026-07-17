#include "MixerProtocol.h"

namespace OpenMix {

// subclasses should override to return their specific capabilities
const MixerCapabilities& MixerProtocol::capabilities() const {
    static MixerCapabilities defaultCaps = MixerCapabilities::forConsole(ConsoleType::Unknown);
    return defaultCaps;
}

// default no-op implementations of the semantic channel setters; drivers that
// support direct channel control override the ones they can encode.
void MixerProtocol::setChannelFaderDb(int, double) {}
void MixerProtocol::setChannelMute(int, bool) {}
void MixerProtocol::setChannelPreamp(int, double) {}
void MixerProtocol::setChannelHpf(int, bool, double) {}
void MixerProtocol::setChannelEqOn(int, bool) {}
void MixerProtocol::setChannelEqBand(int, int, bool, int, double, double, double) {}
void MixerProtocol::setChannelDynamics(int, bool, double, double, double, double, double) {}
void MixerProtocol::setChannelName(int, const QString&) {}
void MixerProtocol::setChannelColor(int, int) {}

void MixerProtocol::setDcaMute(int dca, bool muted) {
    sendParameter(QStringLiteral("/dca/%1/mute").arg(dca), muted ? 1 : 0);
}
void MixerProtocol::setDcaFaderDb(int dca, double level) {
    sendParameter(QStringLiteral("/dca/%1/fader").arg(dca), level);
}
void MixerProtocol::setDcaName(int dca, const QString& name) {
    sendParameter(QStringLiteral("/dca/%1/config/name").arg(dca), name);
}

void MixerProtocol::setChannelDcaMask(int, quint32) {}
void MixerProtocol::setBusDcaMask(int, quint32) {}

} // namespace OpenMix
