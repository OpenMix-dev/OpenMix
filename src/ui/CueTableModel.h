#pragma once

#include <QAbstractTableModel>
#include <QMimeData>

namespace OpenMix {

class CueList;
class Cue;

class CueTableModel : public QAbstractTableModel {
    Q_OBJECT

  public:
    // ColColor is appended last so existing column indices stay stable for views
    // that assign per-column delegates/widths by name.
    // Order mirrors the reference console layout: colour dot, cue number, text,
    // then recall summaries; editing columns (Type/Group/Tags/Notes/Fade) trail
    // and are hidden by default.
    enum Column {
        ColColor = 0, // ● cue colour dot
        ColNumber,    // Cue number
        ColName,      // Text (cue name)
        ColFx,        // read-only muted FX units
        ColSnip,      // read-only console snippet indices
        ColExternal,  // linked external-playback cue (QLab/SCS/Cue Player)
        ColDca,       // read-only summary of DCA overrides (channel labels)
        ColPosition,  // read-only count of positioned channels
        ColType,
        ColGroup,
        ColTags,
        ColNotes,
        ColFade, // fade duration (instant / seconds)
        ColCount
    };

    explicit CueTableModel(CueList* cueList, QObject* parent = nullptr);

    // qAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // editing support
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // drag-and-drop support
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                         const QModelIndex& parent) const override;

    // highlighting for playback
    void setCurrentCueIndex(int index);
    void setStandbyCueIndex(int index);
    int currentCueIndex() const { return m_currentIndex; }
    int standbyCueIndex() const { return m_standbyIndex; }

    // access to underlying data
    CueList* cueList() const { return m_cueList; }
    const Cue* cueAt(int row) const;

  signals:
    void cueReordered(int fromIndex, int toIndex);
    void cueEditRequested(int index, int column, const QVariant& oldValue,
                          const QVariant& newValue);

  private slots:
    void onCueAboutToBeAdded(int index);
    void onCueAdded(int index);
    void onCueAboutToBeRemoved(int index);
    void onCueRemoved(int index);
    void onCueUpdated(int index);
    void onCueAboutToBeMoved(int from, int to);
    void onCueMoved(int from, int to);
    void onListCleared();
    void onListLoaded();

  private:
    CueList* m_cueList;
    int m_currentIndex = -1;
    int m_standbyIndex = -1;

    static const QString s_mimeType;
};

} // namespace OpenMix
