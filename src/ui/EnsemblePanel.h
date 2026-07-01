#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace OpenMix {

class Application;
class EnsembleLibrary;

// Manage the show's ensembles: named groups of channels that share one actor
// profile slot. Mirrors the pop-out panel pattern (Application-backed, refresh()
// on show). Editing writes straight back to the Show-owned EnsembleLibrary.
class EnsemblePanel : public QWidget {
    Q_OBJECT

  public:
    explicit EnsemblePanel(Application* app, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private slots:
    void addEnsemble();
    void removeEnsemble();
    void onSelectionChanged();
    void onNameEdited();
    void onChannelsEdited();
    void onProfileSlotChanged();

  private:
    void setupUi();
    void populateList();
    void updateEditor();
    void setEditorEnabled(bool enabled);
    [[nodiscard]] QString currentEnsembleId() const;

    Application* m_app;
    EnsembleLibrary* m_library = nullptr;

    QListWidget* m_list;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;

    QLineEdit* m_nameEdit;
    QLineEdit* m_channelsEdit;
    QComboBox* m_slotCombo;
    QLabel* m_summaryLabel;

    bool m_updatingUi = false;
};

} // namespace OpenMix
