#pragma once

#include <QJsonObject>
#include <QSet>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace OpenMix {

class Application;
class Actor;
class ActorProfileLibrary;
class VoiceEditorWidget;

// Cast-management panel: an actor tree
// with add/remove/move controls, a per-actor voice editor (gain/HPF/EQ/dynamics)
// for each profile slot plus the spare-mic backup voice, profile-slot management,
// a backup-channel toggle, copy/paste/import/export of actor values, and
// load/save of actor groups. Edits flow straight into the Show's
// ActorProfileLibrary (which marks the show modified and persists in the project).
class ActorSetupPanel : public QWidget {
    Q_OBJECT

  public:
    explicit ActorSetupPanel(Application* app, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private slots:
    // actor list
    void onActorSelectionChanged();
    void addActor();
    void addActors(); // bulk add: paste one actor per line
    void removeActor();
    void moveActorUp();
    void moveActorDown();
    void onActorItemChanged(QTreeWidgetItem* item, int column); // inline edits

    // per-actor fields
    void onNameChanged(const QString& text);
    void onRolesChanged(const QString& text);
    void onChannelChanged(int channel);
    void onActiveToggled(bool on);
    void onUseRoleNameToggled(bool on);
    void onBackupToggled(bool on);

    // profile slots
    void onSlotChanged(int index);
    void addSlot();
    void removeSlot();

    // import/export
    void copyActorValues();
    void pasteActorValues();
    void importActorValues();
    void exportActorValues();
    void saveActorGroup();
    void loadActorGroup();

    void onLibraryChanged();

  private:
    void setupUi();
    void loadDisplayOptions();
    void rebuildActorTree(const QString& selectId = QString());
    void rebuildSlotCombo();
    void loadActorIntoEditor();
    void commitVoiceEdits();
    void setEditorEnabled(bool on);
    void updateButtonStates();

    [[nodiscard]] QString selectedActorId() const;
    [[nodiscard]] int channelCount() const;
    // lowest channel not in used, capped at channelCount(); inserts the result
    [[nodiscard]] int takeLowestFreeChannel(QSet<int>& used) const;

    Application* m_app;
    ActorProfileLibrary* m_library = nullptr;

    bool m_updatingUi = false;
    QString m_currentSlot;
    QJsonObject m_copiedProfiles; // in-app clipboard for copy/paste actor values

    // actor list
    QTreeWidget* m_actorTree = nullptr;
    QPushButton* m_addActorBtn = nullptr;
    QPushButton* m_addActorsBtn = nullptr;
    QPushButton* m_removeActorBtn = nullptr;
    QPushButton* m_moveUpBtn = nullptr;
    QPushButton* m_moveDownBtn = nullptr;

    // toolbar
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_pasteBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_saveGroupBtn = nullptr;
    QPushButton* m_loadGroupBtn = nullptr;

    // editor
    QWidget* m_editor = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_rolesEdit = nullptr;
    QSpinBox* m_channelSpin = nullptr;
    QCheckBox* m_activeCheck = nullptr;
    QCheckBox* m_useRoleCheck = nullptr;
    QCheckBox* m_backupCheck = nullptr;

    QComboBox* m_slotCombo = nullptr;
    QPushButton* m_addSlotBtn = nullptr;
    QPushButton* m_removeSlotBtn = nullptr;

    QTabWidget* m_voiceTabs = nullptr;
    VoiceEditorWidget* m_mainVoice = nullptr;
    VoiceEditorWidget* m_backupVoice = nullptr;

    // display options (persisted to QSettings; consumed by check-cue rendering)
    QCheckBox* m_scribbleNamesCheck = nullptr;
    QCheckBox* m_cueZeroLabelsCheck = nullptr;
};

} // namespace OpenMix
