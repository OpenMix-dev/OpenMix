#pragma once

#include <QDialog>

class QSpinBox;
class QTableWidget;

namespace OpenMix {

class Application;

// Edit the show's Cue Zero base/reset state: an optional base scene plus level
// (path -> value) and label (path -> name) parameters. Recalling Cue Zero returns
// the console to this known starting point; its values double as panic safe-values.
class CueZeroDialog : public QDialog {
    Q_OBJECT

  public:
    explicit CueZeroDialog(Application* app, QWidget* parent = nullptr);

  private slots:
    void addLevelRow();
    void removeLevelRow();
    void addLabelRow();
    void removeLabelRow();
    void applyChanges();

  private:
    void setupUi();
    void load();

    Application* m_app;

    QSpinBox* m_baseSceneSpin;
    QTableWidget* m_levelsTable;
    QTableWidget* m_labelsTable;
};

} // namespace OpenMix
