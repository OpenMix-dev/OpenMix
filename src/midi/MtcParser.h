#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace OpenMix {

// MIDI Time Code frame rate, encoded in the high bits of the hours field.
enum class MtcRate {
    Fps24 = 0,    // 24 fps (film)
    Fps25 = 1,    // 25 fps (EBU)
    Fps2997 = 2,  // 29.97 fps drop-frame
    Fps30 = 3     // 30 fps
};

struct MtcTime {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    MtcRate rate = MtcRate::Fps30;
    bool valid = false;
};

// Assembles MIDI Time Code from the two transports the standard defines:
//
//  * Quarter-frame messages: status 0xF1 followed by one data byte. The data
//    byte is (pieceIndex << 4) | nibble, pieces 0..7. A full hh:mm:ss:ff is
//    spread across all 8 pieces; parseQuarterFrame() returns true once piece 7
//    completes a frame.
//  * Full-frame: a SysEx F0 7F <chan> 01 01 hh mm ss ff F7 carrying a whole
//    timecode in one message (used on locate/seek).
//
// Header-only and dependency-free (mirrors MscParser.h) so it is trivial to
// unit-test the bit assembly in isolation.
class MtcParser {
  public:
    // Feed one quarter-frame DATA byte (the byte after the 0xF1 status).
    // Returns true when this byte was piece 7 and a full frame is now assembled.
    bool parseQuarterFrame(unsigned char dataByte) {
        const int piece = (dataByte >> 4) & 0x07;
        const int nibble = dataByte & 0x0F;
        m_nibbles[piece] = static_cast<uint8_t>(nibble);

        if (piece == 7) {
            assemble();
            return true;
        }
        return false;
    }

    // Parse a MTC full-frame SysEx. Returns a valid MtcTime on success.
    static MtcTime parseFullFrame(const std::vector<unsigned char>& m) {
        MtcTime t;
        if (m.size() < 10 || m[0] != 0xF0 || m[1] != 0x7F || m[3] != 0x01 || m[4] != 0x01 ||
            m.back() != 0xF7) {
            return t;
        }
        const unsigned char rateHour = m[5];
        t.hours = rateHour & 0x1F;
        t.rate = static_cast<MtcRate>((rateHour >> 5) & 0x03);
        t.minutes = m[6] & 0x3F;
        t.seconds = m[7] & 0x3F;
        t.frames = m[8] & 0x1F;
        t.valid = true;
        return t;
    }

    [[nodiscard]] MtcTime time() const { return m_time; }

    void reset() {
        m_nibbles.fill(0);
        m_time = MtcTime{};
    }

  private:
    void assemble() {
        m_time.frames = (m_nibbles[0] & 0x0F) | ((m_nibbles[1] & 0x01) << 4);
        m_time.seconds = (m_nibbles[2] & 0x0F) | ((m_nibbles[3] & 0x03) << 4);
        m_time.minutes = (m_nibbles[4] & 0x0F) | ((m_nibbles[5] & 0x03) << 4);
        const int hourLow = m_nibbles[6] & 0x0F;
        const int hourHighAndRate = m_nibbles[7] & 0x0F;
        m_time.hours = hourLow | ((hourHighAndRate & 0x01) << 4);
        m_time.rate = static_cast<MtcRate>((hourHighAndRate >> 1) & 0x03);
        m_time.valid = true;
    }

    std::array<uint8_t, 8> m_nibbles{};
    MtcTime m_time;
};

} // namespace OpenMix
