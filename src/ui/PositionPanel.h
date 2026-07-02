#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace OpenMix {

class Application;
class PositionLibrary;

// Manage the show's named spatial positions: reusable pan/delay/bus presets a
// cue can assign to channels. Pop-out panel backed by the Show-owned
// PositionLibrary; edits write straight back to it.
class PositionPanel : public QWidget {
    Q_OBJECT

  public:
    explicit PositionPanel(Application* app, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private slots:
    void addPosition();
    void removePosition();
    void onSelectionChanged();
    void onFieldsEdited();

  private:
    void setupUi();
    void populateList();
    void updateEditor();
    void setEditorEnabled(bool enabled);
    [[nodiscard]] QString currentPositionId() const;
    void commitField();

    Application* m_app;
    PositionLibrary* m_library = nullptr;

    QListWidget* m_list;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;

    QLineEdit* m_nameEdit;
    QLineEdit* m_shortNameEdit;
    QDoubleSpinBox* m_delaySpin;
    QDoubleSpinBox* m_panSpin;
    QLineEdit* m_busesEdit;

    bool m_updatingUi = false;
};

} // namespace OpenMix
