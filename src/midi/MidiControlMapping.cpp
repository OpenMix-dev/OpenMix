#include "MidiControlMapping.h"

#include <QStringList>

namespace OpenMix {

bool MidiTrigger::matches(MidiMessageType msgType, int msgChannel, int msgNoteOrCC,
                          int msgValue) const {
    return type == msgType && channel == msgChannel && noteOrCC == msgNoteOrCC &&
           msgValue >= minValue;
}

bool MidiTrigger::operator==(const MidiTrigger& other) const {
    return type == other.type && channel == other.channel && noteOrCC == other.noteOrCC &&
           minValue == other.minValue;
}

QString MidiTrigger::toString() const {
    QString typeStr;
    switch (type) {
    case MidiMessageType::NoteOn:
        typeStr = "Note";
        break;
    case MidiMessageType::NoteOff:
        typeStr = "NoteOff";
        break;
    case MidiMessageType::ControlChange:
        typeStr = "CC";
        break;
    }

    QString result = QString("%1 %2 Ch %3").arg(typeStr).arg(noteOrCC).arg(channel + 1);
    if (minValue > 1) {
        result += QString(" (>=%1)").arg(minValue);
    }
    return result;
}

MidiTrigger MidiTrigger::fromString(const QString& str) {
    MidiTrigger trigger;

    QStringList parts = str.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 4)
        return trigger;

    QString typeStr = parts[0];
    if (typeStr == "Note") {
        trigger.type = MidiMessageType::NoteOn;
    } else if (typeStr == "NoteOff") {
        trigger.type = MidiMessageType::NoteOff;
    } else if (typeStr == "CC") {
        trigger.type = MidiMessageType::ControlChange;
    }

    trigger.noteOrCC = parts[1].toInt();

    // "Ch" is parts[2]
    trigger.channel = parts[3].toInt() - 1;
    if (trigger.channel < 0)
        trigger.channel = 0;
    if (trigger.channel > 15)
        trigger.channel = 15;

    // check for minValue
    if (parts.size() > 4) {
        QString minStr = parts[4];
        if (minStr.startsWith("(>=") && minStr.endsWith(")")) {
            minStr = minStr.mid(3, minStr.length() - 4);
            trigger.minValue = minStr.toInt();
        }
    }

    return trigger;
}

QJsonObject MidiTrigger::toJson() const {
    QJsonObject obj;
    obj["type"] = midiMessageTypeToString(type);
    obj["channel"] = channel;
    obj["noteOrCC"] = noteOrCC;
    obj["minValue"] = minValue;
    return obj;
}

MidiTrigger MidiTrigger::fromJson(const QJsonObject& json) {
    MidiTrigger trigger;
    trigger.type = midiMessageTypeFromString(json["type"].toString());
    trigger.channel = json["channel"].toInt(0);
    trigger.noteOrCC = json["noteOrCC"].toInt(0);
    trigger.minValue = json["minValue"].toInt(1);
    return trigger;
}

bool MidiMapping::operator==(const MidiMapping& other) const {
    return trigger == other.trigger && action == other.action;
}

QJsonObject MidiMapping::toJson() const {
    QJsonObject obj;
    obj["trigger"] = trigger.toJson();
    obj["action"] = midiActionToString(action);
    return obj;
}

MidiMapping MidiMapping::fromJson(const QJsonObject& json) {
    MidiMapping mapping;
    mapping.trigger = MidiTrigger::fromJson(json["trigger"].toObject());
    mapping.action = midiActionFromString(json["action"].toString());
    return mapping;
}

QString midiMessageTypeToString(MidiMessageType type) {
    switch (type) {
    case MidiMessageType::NoteOn:
        return "NoteOn";
    case MidiMessageType::NoteOff:
        return "NoteOff";
    case MidiMessageType::ControlChange:
        return "ControlChange";
    }
    return "NoteOn";
}

MidiMessageType midiMessageTypeFromString(const QString& str) {
    if (str == "NoteOff")
        return MidiMessageType::NoteOff;
    if (str == "ControlChange")
        return MidiMessageType::ControlChange;
    return MidiMessageType::NoteOn;
}

QString midiActionToString(MidiAction action) {
    switch (action) {
    case MidiAction::None:
        return "None";
    case MidiAction::Go:
        return "Go";
    case MidiAction::Stop:
        return "Stop";
    case MidiAction::Pause:
        return "Pause";
    case MidiAction::Resume:
        return "Resume";
    case MidiAction::Previous:
        return "Previous";
    case MidiAction::Next:
        return "Next";
    case MidiAction::GoToFirst:
        return "GoToFirst";
    case MidiAction::GoToLast:
        return "GoToLast";
    case MidiAction::Panic:
        return "Panic";
    case MidiAction::PanicImmediate:
        return "PanicImmediate";
    case MidiAction::PanicAndRestore:
        return "PanicAndRestore";
    case MidiAction::RestoreFromPanic:
        return "RestoreFromPanic";
    }
    return "None";
}

MidiAction midiActionFromString(const QString& str) {
    if (str == "Go")
        return MidiAction::Go;
    if (str == "Stop")
        return MidiAction::Stop;
    if (str == "Pause")
        return MidiAction::Pause;
    if (str == "Resume")
        return MidiAction::Resume;
    if (str == "Previous")
        return MidiAction::Previous;
    if (str == "Next")
        return MidiAction::Next;
    if (str == "GoToFirst")
        return MidiAction::GoToFirst;
    if (str == "GoToLast")
        return MidiAction::GoToLast;
    if (str == "Panic")
        return MidiAction::Panic;
    if (str == "PanicImmediate")
        return MidiAction::PanicImmediate;
    if (str == "PanicAndRestore")
        return MidiAction::PanicAndRestore;
    if (str == "RestoreFromPanic")
        return MidiAction::RestoreFromPanic;
    return MidiAction::None;
}

QString midiActionDisplayName(MidiAction action) {
    switch (action) {
    case MidiAction::None:
        return QObject::tr("None");
    case MidiAction::Go:
        return QObject::tr("GO");
    case MidiAction::Stop:
        return QObject::tr("Stop");
    case MidiAction::Pause:
        return QObject::tr("Pause");
    case MidiAction::Resume:
        return QObject::tr("Resume");
    case MidiAction::Previous:
        return QObject::tr("Previous Cue");
    case MidiAction::Next:
        return QObject::tr("Next Cue");
    case MidiAction::GoToFirst:
        return QObject::tr("Go to First Cue");
    case MidiAction::GoToLast:
        return QObject::tr("Go to Last Cue");
    case MidiAction::Panic:
        return QObject::tr("Panic");
    case MidiAction::PanicImmediate:
        return QObject::tr("Panic (Immediate)");
    case MidiAction::PanicAndRestore:
        return QObject::tr("Panic + Restore");
    case MidiAction::RestoreFromPanic:
        return QObject::tr("Restore from Panic");
    }
    return QObject::tr("None");
}

QVector<MidiAction> allMidiActions() {
    return {MidiAction::None,
            MidiAction::Go,
            MidiAction::Stop,
            MidiAction::Pause,
            MidiAction::Resume,
            MidiAction::Previous,
            MidiAction::Next,
            MidiAction::GoToFirst,
            MidiAction::GoToLast,
            MidiAction::Panic,
            MidiAction::PanicImmediate,
            MidiAction::PanicAndRestore,
            MidiAction::RestoreFromPanic};
}

} // namespace OpenMix
