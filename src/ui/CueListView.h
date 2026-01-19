#pragma once

#include <QTableView>
#include <QWidget>

namespace StageBlend {

class Application;
class Cue;
class CueTableModel;
class CueFilterProxyModel;
class CueFilterBar;

class CueListView : public QWidget {
    Q_OBJECT

  public:
    explicit CueListView(Application* app, QWidget* parent = nullptr);

    int selectedCueIndex() const;
    void setCurrentCueHighlight(int index);
    void setStandbyCueHighlight(int index);
    void refreshAll();
    void refreshCurrentCue();

    // access to filter bar for external updates
    CueFilterBar* filterBar() const { return m_filterBar; }

  public slots:
    void addNewCue();
    void deleteSelectedCue();
    void duplicateSelectedCue();

  signals:
    void cueSelected(int index);
    void cueDoubleClicked(int index);

  private slots:
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex& index);
    void onCueReordered(int fromIndex, int toIndex);
    void onFiltersChanged();

  private:
    void setupUi();

    Application* m_app;
    QTableView* m_tableView;
    CueTableModel* m_model;
    CueFilterProxyModel* m_proxyModel;
    CueFilterBar* m_filterBar;
    int m_currentCueIndex = -1;
    int m_standbyCueIndex = -1;
};

} // namespace StageBlend
