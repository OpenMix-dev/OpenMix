#pragma once

#include <QChar>
#include <QString>
#include <optional>
#include <vector>

namespace OpenMix {

// MIDI Show Control command numbers (subset relevant to cue playback).
namespace Msc {
constexpr int GO = 0x01;
constexpr int STOP = 0x02;
constexpr int RESUME = 0x03;
constexpr int TIMED_GO = 0x04;
constexpr int ALL_OFF = 0x0B;
} // namespace Msc

struct MscMessage {
    bool valid = false;
    int deviceId = 0;
    int command = 0;
    std::optional<double> cueNumber; // present when the GO/STOP names a cue
};

// Parse a System Exclusive byte stream as a MIDI Show Control message.
// Frame: F0 7F <deviceID> 02 <commandFormat> <command> [cueNumber ASCII] ... F7
// The cue number, when present, is ASCII digits/'.' terminated by 0x00 or F7.
inline MscMessage parseMsc(const std::vector<unsigned char>& m) {
    MscMessage r;
    if (m.size() < 6 || m[0] != 0xF0 || m[1] != 0x7F || m[3] != 0x02)
        return r;

    r.deviceId = m[2];
    r.command = m[5];

    QString cueText;
    for (std::size_t i = 6; i < m.size(); ++i) {
        unsigned char b = m[i];
        if (b == 0x00 || b == 0xF7)
            break;
        if ((b >= '0' && b <= '9') || b == '.')
            cueText.append(QChar(b));
        else
            break;
    }
    if (!cueText.isEmpty()) {
        bool ok = false;
        double value = cueText.toDouble(&ok);
        if (ok)
            r.cueNumber = value;
    }

    r.valid = true;
    return r;
}

} // namespace OpenMix
