#include "midi/MscParser.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestMsc : public QObject {
    Q_OBJECT

  private slots:
    void parsesGoWithCueNumber() {
        // F0 7F 7F 02 01 01 '1' '.' '5' 00 F7  -> GO cue 1.5, all devices
        std::vector<unsigned char> m = {0xF0, 0x7F, 0x7F, 0x02, 0x01, Msc::GO,
                                        '1',  '.',  '5',  0x00, 0xF7};
        MscMessage r = parseMsc(m);
        QVERIFY(r.valid);
        QCOMPARE(r.command, Msc::GO);
        QCOMPARE(r.deviceId, 0x7F);
        QVERIFY(r.cueNumber.has_value());
        QVERIFY(qAbs(*r.cueNumber - 1.5) < 1e-9);
    }

    void parsesGoWithoutCueNumber() {
        std::vector<unsigned char> m = {0xF0, 0x7F, 0x05, 0x02, 0x01, Msc::GO, 0xF7};
        MscMessage r = parseMsc(m);
        QVERIFY(r.valid);
        QCOMPARE(r.command, Msc::GO);
        QCOMPARE(r.deviceId, 5);
        QVERIFY(!r.cueNumber.has_value());
    }

    void parsesStop() {
        std::vector<unsigned char> m = {0xF0, 0x7F, 0x00, 0x02, 0x01, Msc::STOP, 0xF7};
        MscMessage r = parseMsc(m);
        QVERIFY(r.valid);
        QCOMPARE(r.command, Msc::STOP);
    }

    void rejectsNonMscSysex() {
        // a non-MSC universal sysex (sub-id != 0x02)
        std::vector<unsigned char> m = {0xF0, 0x7F, 0x00, 0x06, 0x01, 0x01, 0xF7};
        QVERIFY(!parseMsc(m).valid);

        // too short
        std::vector<unsigned char> tooShort = {0xF0, 0x7F, 0x00};
        QVERIFY(!parseMsc(tooShort).valid);

        // not a sysex at all
        std::vector<unsigned char> note = {0x90, 0x3C, 0x40};
        QVERIFY(!parseMsc(note).valid);
    }

    void parsesIntegerCueNumber() {
        std::vector<unsigned char> m = {0xF0, 0x7F, 0x7F, 0x02, 0x01, Msc::GO, '4', '2', 0x00, 0xF7};
        MscMessage r = parseMsc(m);
        QVERIFY(r.cueNumber.has_value());
        QVERIFY(qAbs(*r.cueNumber - 42.0) < 1e-9);
    }
};

QTEST_MAIN(TestMsc)
#include "test_msc.moc"
