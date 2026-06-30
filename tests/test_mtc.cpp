#include "midi/MtcParser.h"
#include <QtTest/QtTest>
#include <vector>

using namespace OpenMix;

class TestMtc : public QObject {
    Q_OBJECT

  private:
    // build the 8 quarter-frame data bytes for a timecode, then feed them.
    static MtcTime feedQuarterFrames(MtcParser& parser, int h, int m, int s, int f, int rate) {
        const int hourField = (h & 0x1F) | ((rate & 0x03) << 5);
        const std::array<int, 8> nibbles = {
            f & 0x0F,         (f >> 4) & 0x0F,         s & 0x0F, (s >> 4) & 0x0F,
            m & 0x0F,         (m >> 4) & 0x0F,         hourField & 0x0F,
            (hourField >> 4) & 0x0F};

        bool completed = false;
        for (int piece = 0; piece < 8; ++piece) {
            const unsigned char dataByte = static_cast<unsigned char>((piece << 4) | nibbles[piece]);
            completed = parser.parseQuarterFrame(dataByte);
        }
        Q_ASSERT(completed);
        return parser.time();
    }

  private slots:
    void quarterFrame_assemblesSmallTimecode() {
        MtcParser parser;
        const MtcTime tc = feedQuarterFrames(parser, 1, 2, 3, 4, /*25fps*/ 1);
        QVERIFY(tc.valid);
        QCOMPARE(tc.hours, 1);
        QCOMPARE(tc.minutes, 2);
        QCOMPARE(tc.seconds, 3);
        QCOMPARE(tc.frames, 4);
        QCOMPARE(static_cast<int>(tc.rate), 1); // Fps25
    }

    void quarterFrame_assemblesMaxFields() {
        MtcParser parser;
        const MtcTime tc = feedQuarterFrames(parser, 23, 58, 59, 29, /*30fps*/ 3);
        QVERIFY(tc.valid);
        QCOMPARE(tc.hours, 23);
        QCOMPARE(tc.minutes, 58);
        QCOMPARE(tc.seconds, 59);
        QCOMPARE(tc.frames, 29);
        QCOMPARE(static_cast<int>(tc.rate), 3); // Fps30
    }

    void quarterFrame_onlyCompletesOnPiece7() {
        MtcParser parser;
        // pieces 0..6 must not report completion
        for (int piece = 0; piece < 7; ++piece) {
            const unsigned char dataByte = static_cast<unsigned char>((piece << 4) | 0x00);
            QVERIFY(!parser.parseQuarterFrame(dataByte));
        }
        // piece 7 completes the frame
        QVERIFY(parser.parseQuarterFrame(static_cast<unsigned char>((7 << 4) | 0x00)));
    }

    void fullFrame_parsesSysex() {
        // F0 7F 7F 01 01 <rate|hh> mm ss ff F7 for 23:58:59:29 @ 30fps
        const int rateHour = (3 << 5) | 23;
        const std::vector<unsigned char> sysex = {
            0xF0, 0x7F, 0x7F, 0x01, 0x01, static_cast<unsigned char>(rateHour),
            58,   59,   29,   0xF7};
        const MtcTime tc = MtcParser::parseFullFrame(sysex);
        QVERIFY(tc.valid);
        QCOMPARE(tc.hours, 23);
        QCOMPARE(tc.minutes, 58);
        QCOMPARE(tc.seconds, 59);
        QCOMPARE(tc.frames, 29);
        QCOMPARE(static_cast<int>(tc.rate), 3);
    }

    void fullFrame_rejectsMalformed() {
        const std::vector<unsigned char> tooShort = {0xF0, 0x7F, 0xF7};
        QVERIFY(!MtcParser::parseFullFrame(tooShort).valid);

        std::vector<unsigned char> wrongSubId = {0xF0, 0x7F, 0x7F, 0x02, 0x01,
                                                 0x00, 0x00, 0x00, 0x00, 0xF7};
        QVERIFY(!MtcParser::parseFullFrame(wrongSubId).valid);
    }
};

QTEST_MAIN(TestMtc)
#include "test_mtc.moc"
