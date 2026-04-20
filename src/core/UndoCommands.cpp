#include "UndoCommands.h"
#include "CueList.h"

namespace OpenMix {

EditCueCommand::EditCueCommand(CueList* cueList, int index, const Cue& oldCue, const Cue& newCue,
                               QUndoCommand* parent)
    : QUndoCommand(parent), m_cueList(cueList), m_index(index), m_oldCue(oldCue), m_newCue(newCue) {
    setText(QObject::tr("Edit Cue %1").arg(oldCue.number(), 0, 'f', 1));
}

void EditCueCommand::undo() {
    if (m_cueList && m_index >= 0 && m_index < m_cueList->count()) {
        m_cueList->updateCue(m_index, m_oldCue);
    }
}

void EditCueCommand::redo() {
    if (m_firstRedo) {
        // edit already applied
        m_firstRedo = false;
        return;
    }

    if (m_cueList && m_index >= 0 && m_index < m_cueList->count()) {
        m_cueList->updateCue(m_index, m_newCue);
    }
}

bool EditCueCommand::mergeWith(const QUndoCommand* other) {
    if (other->id() != id())
        return false;

    const EditCueCommand* cmd = static_cast<const EditCueCommand*>(other);

    // only merge if editing the same cue
    if (cmd->m_index != m_index || cmd->m_cueList != m_cueList) {
        return false;
    }

    // keep original old state, use new command's new state
    m_newCue = cmd->m_newCue;
    return true;
}

AddCueCommand::AddCueCommand(CueList* cueList, const Cue& cue, int index, QUndoCommand* parent)
    : QUndoCommand(parent), m_cueList(cueList), m_cue(cue), m_index(index) {
    setText(QObject::tr("Add Cue %1").arg(cue.number(), 0, 'f', 1));
}

void AddCueCommand::undo() {
    if (m_cueList) {
        int idx = m_index >= 0 ? m_index : m_cueList->count() - 1;
        if (idx >= 0)
            m_cueList->removeCue(idx);
    }
}

void AddCueCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }

    if (m_cueList) {
        if (m_index >= 0) {
            m_cueList->insertCue(m_index, m_cue);
        } else {
            m_cueList->addCue(m_cue);
        }
    }
}

RemoveCueCommand::RemoveCueCommand(CueList* cueList, int index, QUndoCommand* parent)
    : QUndoCommand(parent), m_cueList(cueList), m_index(index) {
    if (m_cueList && index >= 0 && index < m_cueList->count()) {
        m_cue = m_cueList->at(index);
        setText(QObject::tr("Delete Cue %1").arg(m_cue.number(), 0, 'f', 1));
    } else {
        setText(QObject::tr("Delete Cue"));
    }
}

void RemoveCueCommand::undo() {
    if (m_cueList) {
        m_cueList->insertCue(m_index, m_cue);
    }
}

void RemoveCueCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }

    if (m_cueList && m_index >= 0 && m_index < m_cueList->count()) {
        m_cueList->removeCue(m_index);
    }
}

MoveCueCommand::MoveCueCommand(CueList* cueList, int fromIndex, int toIndex, QUndoCommand* parent)
    : QUndoCommand(parent), m_cueList(cueList), m_fromIndex(fromIndex), m_toIndex(toIndex) {
    if (m_cueList && fromIndex >= 0 && fromIndex < m_cueList->count()) {
        const Cue& cue = m_cueList->at(fromIndex);
        setText(QObject::tr("Move Cue %1").arg(cue.number(), 0, 'f', 1));
    } else {
        setText(QObject::tr("Move Cue"));
    }
}

void MoveCueCommand::undo() {
    if (m_cueList) {
        m_cueList->moveCue(m_toIndex, m_fromIndex);
    }
}

void MoveCueCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }

    if (m_cueList) {
        m_cueList->moveCue(m_fromIndex, m_toIndex);
    }
}

BatchEditCommand::BatchEditCommand(CueList* cueList, const QVector<int>& indices,
                                   const QVector<Cue>& oldCues, const QVector<Cue>& newCues,
                                   const QString& text, QUndoCommand* parent)
    : QUndoCommand(parent), m_cueList(cueList), m_indices(indices), m_oldCues(oldCues),
      m_newCues(newCues) {
    Q_ASSERT(indices.size() == oldCues.size() && oldCues.size() == newCues.size());
    setText(text);
}

void BatchEditCommand::undo() {
    if (!m_cueList)
        return;

    for (int i = 0; i < m_indices.size(); ++i) {
        int idx = m_indices[i];
        if (idx >= 0 && idx < m_cueList->count()) {
            m_cueList->updateCue(idx, m_oldCues[i]);
        }
    }
}

void BatchEditCommand::redo() {
    if (m_firstRedo) {
        m_firstRedo = false;
        return;
    }

    if (!m_cueList)
        return;

    for (int i = 0; i < m_indices.size(); ++i) {
        int idx = m_indices[i];
        if (idx >= 0 && idx < m_cueList->count()) {
            m_cueList->updateCue(idx, m_newCues[i]);
        }
    }
}

} // namespace OpenMix
