#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace OpenMix {

enum class MidiMessageType { NoteOn, NoteOff, ControlChange };

enum class MidiAction {
    None,
    Go,
    Stop,
    Pause,
    Resume,
    Previous,
    Next,
    GoToFirst,
    GoToLast,
    Panic,
    PanicImmediate,
    PanicAndRestore,
    RestoreFromPanic
};

struct MidiTrigger {
    MidiMessageType type = MidiMessageType::NoteOn;
    int channel = 0;  // 0-15
    int noteOrCC = 0; // 0-127
    int minValue = 1; // minimum velocity/CC value to trigger (0-127)

    bool matches(MidiMessageType msgType, int msgChannel, int msgNoteOrCC, int msgValue) const;

    bool operator==(const MidiTrigger& other) const;
    bool operator!=(const MidiTrigger& other) const { return !(*this == other); }

    QString toString() const;
    static MidiTrigger fromString(const QString& str);

    QJsonObject toJson() const;
    static MidiTrigger fromJson(const QJsonObject& json);
};

struct MidiMapping {
    MidiTrigger trigger;
    MidiAction action = MidiAction::None;

    bool operator==(const MidiMapping& other) const;
    bool operator!=(const MidiMapping& other) const { return !(*this == other); }

    QJsonObject toJson() const;
    static MidiMapping fromJson(const QJsonObject& json);
};

// Binds a MIDI trigger to a mixer input channel; firing it toggles that
// channel's mute (a console-style mute button). Kept separate from MidiMapping
// so the parameterless action mappings stay untouched.
struct MidiMuteAssignment {
    MidiTrigger trigger;
    int channel = 0; // 1-based mixer input channel

    bool operator==(const MidiMuteAssignment& other) const;
    bool operator!=(const MidiMuteAssignment& other) const { return !(*this == other); }

    QJsonObject toJson() const;
    static MidiMuteAssignment fromJson(const QJsonObject& json);
};

QString midiMessageTypeToString(MidiMessageType type);
MidiMessageType midiMessageTypeFromString(const QString& str);

QString midiActionToString(MidiAction action);
MidiAction midiActionFromString(const QString& str);
QString midiActionDisplayName(MidiAction action);
QVector<MidiAction> allMidiActions();

} // namespace OpenMix
