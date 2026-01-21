#pragma once

#include "Cue.h"
#include <QJsonObject>
#include <QUndoCommand>
#include <QVariant>

namespace OpenMix {

class CueList;

// command IDs for merging
enum CommandId { EditCueId = 1, BatchEditId = 2, LiveEditId = 3, CommitLiveEditsId = 4 };

// command for editing a single cue
class EditCueCommand : public QUndoCommand {
  public:
    EditCueCommand(CueList* cueList, int index, const Cue& oldCue, const Cue& newCue,
                   QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override { return EditCueId; }
    bool mergeWith(const QUndoCommand* other) override;

  private:
    CueList* m_cueList;
    int m_index;
    Cue m_oldCue;
    Cue m_newCue;
    bool m_firstRedo = true;
};

// command for adding a cue
class AddCueCommand : public QUndoCommand {
  public:
    AddCueCommand(CueList* cueList, const Cue& cue, int index = -1, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

  private:
    CueList* m_cueList;
    Cue m_cue;
    int m_index;
    bool m_firstRedo = true;
};

// command for removing a cue
class RemoveCueCommand : public QUndoCommand {
  public:
    RemoveCueCommand(CueList* cueList, int index, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

  private:
    CueList* m_cueList;
    Cue m_cue;
    int m_index;
    bool m_firstRedo = true;
};

// command for moving a cue (drag-drop reorder)
class MoveCueCommand : public QUndoCommand {
  public:
    MoveCueCommand(CueList* cueList, int fromIndex, int toIndex, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

  private:
    CueList* m_cueList;
    int m_fromIndex;
    int m_toIndex;
    bool m_firstRedo = true;
};

// command for batch operations
class BatchEditCommand : public QUndoCommand {
  public:
    BatchEditCommand(CueList* cueList, const QVector<int>& indices, const QVector<Cue>& oldCues,
                     const QVector<Cue>& newCues, const QString& text,
                     QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

  private:
    CueList* m_cueList;
    QVector<int> m_indices;
    QVector<Cue> m_oldCues;
    QVector<Cue> m_newCues;
    bool m_firstRedo = true;
};

// command for single live edit parameter change
class LiveEditCommand : public QUndoCommand {
  public:
    LiveEditCommand(CueList* cueList, const QString& cueId, const QString& paramPath,
                    const QVariant& oldValue, const QVariant& newValue,
                    QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override { return LiveEditId; }
    bool mergeWith(const QUndoCommand* other) override;

  private:
    CueList* m_cueList;
    QString m_cueId;
    QString m_paramPath;
    QVariant m_oldValue;
    QVariant m_newValue;
    bool m_firstRedo = true;
};

// command for committing batch of live edits to a cue
class CommitLiveEditsCommand : public QUndoCommand {
  public:
    CommitLiveEditsCommand(CueList* cueList, const QString& cueId, const QJsonObject& oldParams,
                           const QJsonObject& newParams, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

  private:
    CueList* m_cueList;
    QString m_cueId;
    QJsonObject m_oldParams;
    QJsonObject m_newParams;
    bool m_firstRedo = true;
};

} // namespace OpenMix
