#pragma once

#include <QWidget>

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QTextEdit;
class QCheckBox;
class QPushButton;
class QFormLayout;
class QLabel;
class QListWidget;
class QGroupBox;
class QScrollArea;
class QStringListModel;
class QVBoxLayout;
class QGridLayout;
class QTableWidget;

namespace OpenMix {

class Application;
class ActorProfileLibrary;
class Cue;
class CollapsibleSection;
class DCAOverrideStrip;

class CueEditor : public QWidget {
    Q_OBJECT

  public:
    explicit CueEditor(Application* app, QWidget* parent = nullptr);

    bool hasFocus() const;

    void addBottomWidget(QWidget* widget);

  public slots:
    void setCue(int index);

  signals:
    void cueModified();

  private slots:
    void onNumberChanged(double value);
    void onNameChanged(const QString& text);
    void onTypeChanged(int index);
    void onAutoFollowChanged(bool checked);
    void onAutoFollowDelayChanged(double value);
    void onNotesChanged();
    void onTargetedDCAsChanged();
    void onDCAOverrideChanged(int dca);
    void onDCALabelCommitted(int dca);

    void onFadeTimeChanged(double value);
    void onFadeCurveChanged(int index);
    void onQLabCueChanged(const QString& text);
    void onChannelProfileChanged(int channel);
    void onChannelPositionChanged(int channel);
    void onChannelLevelToggled(int channel, bool on);
    void onChannelLevelChanged(int channel);
    void onActorLibraryChanged();

    void onColorChanged(const QString& text);
    void onColorPick();
    void onSkipChanged(bool checked);
    void onSnippetsChanged(const QString& text);
    void onFxMuteChanged();
    void onGangsChanged();
    void onCheckModeToggled(bool checked);
    void onEngineCheckModeChanged(bool enabled);
    void onDcaCountChanged(int count);

  private:
    void setupUi();
    void createDCATargetingSection();
    void rebuildDcaTargetChecks(int count);
    void rebuildDcaOverrideStrips(int count);
    void createFadeSection();
    void createChannelProfilesSection();
    void createFxMutesSection();
    void rebuildChannelTable();
    void populateChannelTable();
    void updateFromCue();
    void updateDCAOverridesUI();
    void updateDCAAssignInfo();
    void updateFxMutesUI();
    void updateGangsUI();
    void setEnabled(bool enabled);
    Cue* currentCue();

    Application* m_app;
    int m_currentIndex = -1;
    bool m_updatingUi = false;

    QVBoxLayout* m_mainLayout = nullptr;

    // widgets
    QDoubleSpinBox* m_numberSpin;
    QLineEdit* m_nameEdit;
    QComboBox* m_typeCombo;
    QCheckBox* m_autoFollowCheck;
    QDoubleSpinBox* m_autoFollowDelaySpin;
    QTextEdit* m_notesEdit;

    // fade transition
    QDoubleSpinBox* m_fadeTimeSpin = nullptr;
    QComboBox* m_fadeCurveCombo = nullptr;

    // linked QLab (DAW remote) cue id
    QLineEdit* m_qLabCueEdit = nullptr;

    // per-cue color + skip flag
    QLineEdit* m_colorEdit = nullptr;
    QPushButton* m_colorPickButton = nullptr;
    QCheckBox* m_skipCheck = nullptr;

    // console snippets recalled on fire (comma-separated indices)
    QLineEdit* m_snippetsEdit = nullptr;

    // per-FX-unit mute overrides
    struct FxMuteWidgets {
        QCheckBox* enable;
        QCheckBox* muted;
    };
    CollapsibleSection* m_fxMutesGroup = nullptr;
    QVector<FxMuteWidgets> m_fxMuteWidgets;

    // L/R gangs (show-level) + soundcheck (check) mode toggle
    QLineEdit* m_gangEdit = nullptr;
    QCheckBox* m_checkModeCheck = nullptr;

    // per-channel actor profile slot + level
    ActorProfileLibrary* m_actorLibrary = nullptr;
    CollapsibleSection* m_channelProfilesGroup = nullptr;
    QTableWidget* m_channelTable = nullptr;

    // DCA targeting widgets (rebuilt when the effective DCA count changes)
    CollapsibleSection* m_dcaTargetingGroup = nullptr;
    QCheckBox* m_targetAllDCAsCheck;
    QGridLayout* m_dcaTargetGrid = nullptr;
    QVector<QCheckBox*> m_dcaTargetChecks;

    // DCA overrides (mute/label per DCA), one strip per DCA
    CollapsibleSection* m_dcaOverridesGroup = nullptr;
    QVBoxLayout* m_dcaOverridesContentLayout = nullptr;
    QVector<DCAOverrideStrip*> m_dcaOverrideStrips;

    // role/actor names offered while typing a DCA label; shared by all fields
    QStringListModel* m_actorCompletionModel = nullptr;
};

} // namespace OpenMix
