#pragma once

#include <QDialog>

class QSpinBox;
class QTableWidget;

namespace OpenMix {

class Application;

// Edit the show's FX units (index -> name), per-channel FX assignments, and the
// default FX unit. Writes back to the show's FxLibrary on accept.
class FxSetupDialog : public QDialog {
    Q_OBJECT

  public:
    explicit FxSetupDialog(Application* app, QWidget* parent = nullptr);

  private slots:
    void addUnitRow();
    void removeUnitRow();
    void addAssignRow();
    void removeAssignRow();
    void applyChanges();

  private:
    void setupUi();
    void load();

    Application* m_app;
    QTableWidget* m_unitsTable;
    QTableWidget* m_assignTable;
    QSpinBox* m_defaultSpin;
};

} // namespace OpenMix
