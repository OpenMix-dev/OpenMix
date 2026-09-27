#include "GLDProtocol.h"
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace OpenMix {

namespace {

// Fader level table (GLD MIDI and TCP/IP Protocol V1.4), as <dBu, LV>. Distinct
// from Qu's table: 0 dB is 0x6B here and 0x62 there.
struct LevelPoint {
    double dB;
    quint8 lv;
};

constexpr LevelPoint kGldLevels[] = {
    {-45, 0x11}, {-40, 0x1B}, {-35, 0x25}, {-30, 0x2F}, {-25, 0x39}, {-20, 0x43},
    {-15, 0x4D}, {-10, 0x57}, {-5, 0x61},  {0, 0x6B},   {5, 0x74},   {10, 0x7F},
};

} // namespace

GLDProtocol::GLDProtocol(const MixerCapabilities& caps, QObject* parent)
    : AllenHeathMidiProtocol(caps, parent) {
    initializeSnapshotParams();
}

void GLDProtocol::initializeSnapshotParams() {
    m_snapshotParams.clear();

    for (int i = 1; i <= m_capabilities.dcaCount; ++i) {
        m_snapshotParams.append(dcaFaderPath(i));
        m_snapshotParams.append(dcaMutePath(i));
    }
    for (int i = 1; i <= m_capabilities.inputChannels; ++i) {
        m_snapshotParams.append(QString("/ch/%1/fader").arg(i));
        m_snapshotParams.append(QString("/ch/%1/mute").arg(i));
    }
}

QString GLDProtocol::dcaFaderPath(int dca) const { return QString("/dca/%1/fader").arg(dca); }

QString GLDProtocol::dcaMutePath(int dca) const { return QString("/dca/%1/mute").arg(dca); }

double GLDProtocol::dbFromLevel(int lv) {
    if (lv <= 0) {
        return NEG_INF_DB;
    }
    const auto* first = std::begin(kGldLevels);
    const auto* last = std::end(kGldLevels) - 1;
    if (lv <= first->lv) {
        return first->dB;
    }
    if (lv >= last->lv) {
        return last->dB;
    }
    for (const LevelPoint* p = first; p < last; ++p) {
        const LevelPoint* next = p + 1;
        if (lv >= p->lv && lv <= next->lv) {
            const double span = next->lv - p->lv;
            const double t = span > 0.0 ? (lv - p->lv) / span : 0.0;
            return p->dB + t * (next->dB - p->dB);
        }
    }
    return last->dB;
}

int GLDProtocol::levelFromDb(double dB) {
    if (dB <= NEG_INF_DB) {
        return 0x00;
    }

    const auto* first = std::begin(kGldLevels);
    const auto* last = std::end(kGldLevels) - 1;
    if (dB <= first->dB) {
        return first->lv;
    }
    if (dB >= last->dB) {
        return last->lv;
    }
    for (const LevelPoint* p = first; p < last; ++p) {
        const LevelPoint* next = p + 1;
        if (dB >= p->dB && dB <= next->dB) {
            const double span = next->dB - p->dB;
            const double t = span > 0.0 ? (dB - p->dB) / span : 0.0;
            return static_cast<int>(std::lround(p->lv + t * (next->lv - p->lv)));
        }
    }
    return last->lv;
}

QByteArray GLDProtocol::sysexHeader() const {
    QByteArray h = QByteArray::fromHex("f000001a50100100");
    h.append(static_cast<char>(m_midiChannel & 0x0F));
    return h;
}

QByteArray GLDProtocol::buildFader(int channelId, double dB) const {
    // BN 63 CH | BN 62 17 | BN 06 LV - three messages: unlike SQ and Qu, the
    // fader carries no data-entry LSB
    const char status = static_cast<char>(0xB0 | (m_midiChannel & 0x0F));
    QByteArray msg;
    msg.append(status);
    msg.append(static_cast<char>(0x63));
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(status);
    msg.append(static_cast<char>(0x62));
    msg.append(static_cast<char>(ID_FADER));
    msg.append(status);
    msg.append(static_cast<char>(0x06));
    msg.append(static_cast<char>(levelFromDb(dB) & 0x7F));
    return msg;
}

QByteArray GLDProtocol::buildMute(int channelId, bool muted) const {
    // a Note On carrying the state, then a Note Off. Received velocity 40-7F
    // mutes and 01-3F unmutes; velocity 0 and Note Off are ignored, so the state
    // has to ride the first message.
    const char status = static_cast<char>(0x90 | (m_midiChannel & 0x0F));
    QByteArray msg;
    msg.append(status);
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(static_cast<char>(muted ? 0x7F : 0x3F));
    msg.append(status);
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(static_cast<char>(0x00));
    return msg;
}

QByteArray GLDProtocol::buildName(int channelId, const QString& name) const {
    // header + 03 CH <ascii> F7
    QByteArray msg = sysexHeader();
    msg.append(static_cast<char>(0x03));
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(name.left(MAX_NAME_LENGTH).toLatin1());
    msg.append(static_cast<char>(0xF7));
    return msg;
}

QByteArray GLDProtocol::buildColour(int channelId, int colour) const {
    // header + 06 CH Col F7
    QByteArray msg = sysexHeader();
    msg.append(static_cast<char>(0x06));
    msg.append(static_cast<char>(channelId & 0x7F));
    msg.append(static_cast<char>(std::clamp(colour, COLOUR_OFF, COLOUR_MAX)));
    msg.append(static_cast<char>(0xF7));
    return msg;
}

void GLDProtocol::setChannelName(int channel, const QString& name) {
    if (!isConnected() || channel < 1 || channel > m_capabilities.inputChannels) {
        return;
    }
    m_transport.send(buildName(CH_INPUT_BASE + channel - 1, name));
}

void GLDProtocol::setChannelColor(int channel, int color) {
    if (!isConnected() || channel < 1 || channel > m_capabilities.inputChannels) {
        return;
    }
    m_transport.send(buildColour(CH_INPUT_BASE + channel - 1, color));
}

QString GLDProtocol::pathForChannelId(int channelId, const QString& param) const {
    if (channelId >= CH_INPUT_BASE && channelId < CH_INPUT_BASE + m_capabilities.inputChannels) {
        return QString("/ch/%1/%2").arg(channelId - CH_INPUT_BASE + 1).arg(param);
    }
    if (channelId >= CH_DCA_BASE && channelId < CH_DCA_BASE + m_capabilities.dcaCount) {
        return QString("/dca/%1/%2").arg(channelId - CH_DCA_BASE + 1).arg(param);
    }
    return {};
}

void GLDProtocol::handleNrpn(int msb, int lsb, int dataMsb, int dataLsb) {
    Q_UNUSED(dataLsb); // the GLD fader carries no data-entry LSB
    if (lsb != ID_FADER) {
        return;
    }
    const QString path = pathForChannelId(msb, "fader");
    if (path.isEmpty()) {
        return;
    }
    const double dB = dbFromLevel(dataMsb);
    reportParameter(path, dB);
    if (msb >= CH_INPUT_BASE) {
        emit channelFaderChanged(msb - CH_INPUT_BASE + 1, dB);
    }
}

void GLDProtocol::handleNoteOn(int note, int velocity) {
    // velocity 40-7F = mute on, 01-3F = mute off, 00 = the trailing note off
    if (velocity == 0) {
        return;
    }
    const QString path = pathForChannelId(note, "mute");
    if (!path.isEmpty()) {
        reportParameter(path, velocity >= 0x40);
    }
}

void GLDProtocol::sendParameter(const QString& path, const QVariant& value) {
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
    if (parts[0] == "ch" && index <= m_capabilities.inputChannels) {
        channelId = CH_INPUT_BASE + index - 1;
    } else if (parts[0] == "dca" && index <= m_capabilities.dcaCount) {
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
