#pragma once

#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace StageBlend {

class Cue;
class CueList;

class MacroPreviewWidget : public QWidget {
    Q_OBJECT

  public:
    explicit MacroPreviewWidget(QWidget* parent = nullptr);

    // set the macro cue to preview
    void setMacroCue(const Cue* cue, const CueList* cueList);
    void clear();

    // get the currently displayed cue
    const Cue* currentCue() const { return m_cue; }

  signals:
    void childCueClicked(const QString& cueId);
    void childCueDoubleClicked(const QString& cueId);

  private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

  private:
    void buildTree(QTreeWidgetItem* parent, const Cue* cue, const CueList* cueList, int depth,
                   QSet<QString>& visited);
    QString cueTypeIcon(const Cue* cue) const;
    QString executionModeString(const Cue* cue) const;

    const Cue* m_cue = nullptr;
    const CueList* m_cueList = nullptr;

    QVBoxLayout* m_layout;
    QLabel* m_headerLabel;
    QTreeWidget* m_treeWidget;
    QLabel* m_emptyLabel;
};

} // namespace StageBlend
