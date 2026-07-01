#include "midi/MidiControlMapping.h"
#include <QtTest/QtTest>

using namespace OpenMix;

class TestMidiMute : public QObject {
    Q_OBJECT

  private slots:
    void muteAssignment_roundtrip() {
        MidiMuteAssignment mute;
        mute.trigger.type = MidiMessageType::NoteOn;
        mute.trigger.channel = 2;
        mute.trigger.noteOrCC = 48;
        mute.trigger.minValue = 1;
        mute.channel = 7;

        MidiMuteAssignment loaded = MidiMuteAssignment::fromJson(mute.toJson());
        QCOMPARE(loaded.channel, 7);
        QCOMPARE(loaded.trigger.channel, 2);
        QCOMPARE(loaded.trigger.noteOrCC, 48);
        QVERIFY(loaded.trigger.type == MidiMessageType::NoteOn);
        QVERIFY(loaded == mute);
    }

    void muteAssignment_triggerMatches() {
        MidiMuteAssignment mute;
        mute.trigger.type = MidiMessageType::ControlChange;
        mute.trigger.channel = 0;
        mute.trigger.noteOrCC = 10;
        mute.trigger.minValue = 64;
        mute.channel = 3;

        // matches only a CC on channel 0, controller 10, value >= 64
        QVERIFY(mute.trigger.matches(MidiMessageType::ControlChange, 0, 10, 100));
        QVERIFY(!mute.trigger.matches(MidiMessageType::ControlChange, 0, 10, 20));
        QVERIFY(!mute.trigger.matches(MidiMessageType::NoteOn, 0, 10, 100));
    }

    void muteAssignment_inequality() {
        MidiMuteAssignment a;
        a.channel = 1;
        MidiMuteAssignment b;
        b.channel = 2;
        QVERIFY(a != b);
    }
};

QTEST_MAIN(TestMidiMute)
#include "test_midi_mute.moc"
