#include "QuProtocol.h"
#include <QtGlobal>
#include <cmath>

namespace OpenMix {

QuProtocol::QuProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathMidiProtocol(caps, parent) {
    initializeSnapshotParams();
}

void QuProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        m_snapshotParams.append(dcaFaderPath(i));
        m_snapshotParams.append(dcaMutePath(i));
    }

    // input-channel faders, so a cue restores channel levels (not just DCAs)
    for (int i = 1; i <= m_capabilities.inputChannels; ++i) {
        m_snapshotParams.append(QString("/ch/%1/fader").arg(i));
        m_snapshotParams.append(QString("/ch/%1/mute").arg(i));
    }
}

QString QuProtocol::dcaFaderPath(int dca) const { return QString("/dca/%1/fader").arg(dca); }

QString QuProtocol::dcaMutePath(int dca) const { return QString("/dca/%1/mute").arg(dca); }

namespace {

// Fader / Send Level table (Qu MIDI Protocol V1.9+), as <dBu, VA>. Qu has no
// NRPN Fader Law setting: this one curve is the whole story.
struct LevelPoint {
    double dB;
    quint8 va;
};

constexpr LevelPoint kQuLevels[] = {
    {-45, 0x0C}, {-40, 0x10}, {-35, 0x17}, {-30, 0x1F}, {-25, 0x27}, {-20, 0x2F},
    {-15, 0x36}, {-10, 0x3F}, {-5, 0x4F},  {0, 0x62},   {5, 0x72},   {10, 0x7F},
};

} // namespace

int QuProtocol::levelFromDb(double dB) {
    if (dB <= NEG_INF_DB) {
        return 0x00;
    }

    const auto* first = std::begin(kQuLevels);
    const auto* last = std::end(kQuLevels) - 1;
    if (dB <= first->dB) {
        return first->va;
    }
    if (dB >= last->dB) {
        return last->va;
    }
    for (const LevelPoint* p = first; p < last; ++p) {
        const LevelPoint* next = p + 1;
        if (dB >= p->dB && dB <= next->dB) {
            const double span = next->dB - p->dB;
            const double t = span > 0.0 ? (dB - p->dB) / span : 0.0;
            return static_cast<int>(std::lround(p->va + t * (next->va - p->va)));
        }
    }
    return last->va;
}

QByteArray QuProtocol::buildFader(int channelId, double dB) {
    // BN 63 <CH> | BN 62 17 | BN 06 <VA> | BN 26 07
    return buildNRPNMessage(0, channelId, ID_FADER, levelFromDb(dB), ID_FADER_VX);
}

QByteArray QuProtocol::buildMute(int channelId, bool muted) {
    // a Note On carrying the state, then a Note Off. Received velocity 40-7F
    // mutes and 01-3F unmutes; velocity 0 and Note Off are ignored, so the state
    // has to ride the first message.
    QByteArray msg;
    msg.append(static_cast<char>(0x90));
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(static_cast<char>(muted ? 0x7F : 0x3F));
    msg.append(static_cast<char>(0x90));
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(static_cast<char>(0x00));
    return msg;
}

QByteArray QuProtocol::buildSceneRecall(int sceneNumber) {
    // Bank Select MSB + LSB then the program; Qu has one bank of 100 scenes
    if (sceneNumber < 1) {
        return {};
    }
    QByteArray msg;
    msg.append(static_cast<char>(0xB0));
    msg.append(static_cast<char>(0x00));
    msg.append(static_cast<char>(0x00));
    msg.append(static_cast<char>(0xB0));
    msg.append(static_cast<char>(0x20));
    msg.append(static_cast<char>(0x00));
    msg.append(static_cast<char>(0xC0));
    msg.append(static_cast<char>((sceneNumber - 1) & 0x7F));
    return msg;
}

void QuProtocol::sendParameter(const QString& path, const QVariant& value) {
    if (!isConnected()) {
        return;
    }

    const QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return;
    }

    bool ok = false;
    const int index = parts[1].toInt(&ok);
    if (!ok || index < 1) {
        return;
    }

    int channelId = -1;
    if (parts[0] == "ch" && index <= 32) {
        channelId = CH_INPUT_BASE + index - 1;
    } else if (parts[0] == "dca" && index <= 4) {
        channelId = CH_DCA_BASE + index - 1;
    }
    if (channelId < 0) {
        return;
    }

    const QString param = parts[2];
    if (param == "fader") {
        m_transport.send(buildFader(channelId, value.toDouble()));
    } else if (param == "mute") {
        m_transport.send(buildMute(channelId, value.toBool()));
    }
}

} // namespace OpenMix
