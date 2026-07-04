#pragma once

#include <QWidget>

class QAction;
class QCheckBox;
class QComboBox;
class QEvent;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;

namespace OpenMix {

class Application;
class Cue;
class DCAMapping;

class DCAMappingPanel : public QWidget {
    Q_OBJECT

  public:
    explicit DCAMappingPanel(Application* app, QWidget* parent = nullptr);

  public slots:
    void refresh();
    void syncFromMixer();
    void saveMappingPreset();
    void loadMappingPreset();
    void clearAllMappings();

    // cue-specific mapping
    void setCurrentCue(Cue* cue);
    void clearCurrentCue();

  private slots:
    void onChannelDCAChanged(int channel, int dcaIndex);
    void onBusDCAChanged(int bus, int dcaIndex);
    void onMixerConnected();
    void onMixerDisconnected();

    // cue mapping slots
    void onUseCueMappingToggled(bool enabled);
    void copyShowMappingToCue();
    void clearCueMapping();

    void onActorsChanged();

    // bus name editing
    void startBusNameEdit(int bus);
    void finishBusNameEdit(int bus);
    void cancelBusNameEdit(int bus);

    // channel (actor) name editing
    void startChannelNameEdit(int channel);
    void finishChannelNameEdit(int channel);
    void cancelChannelNameEdit(int channel);

  signals:
    // prev/next buttons: ask the cue list to move its selection, so the whole
    // app follows via the normal cueSelected cascade
    void adjacentCueRequested(int delta);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void setupUi();
    void createChannelSection();
    void createBusSection();
    void createDcaOverviewSection();
    void updateDCAOptions();
    void populateFromMapping();
    void updateComboItemStates();
    void updateContextHeader();
    void updateDcaOverview();
    QString busDisplayName(int bus) const;
    QString channelDisplayName(int channel) const;
    QString dcaDisplayName(int dca) const;

    Application* m_app;
    DCAMapping* m_mapping;

    // cue-specific mapping
    Cue* m_currentCue = nullptr;
    bool m_showingCueMapping = false;

    // UI elements
    QWidget* m_contextHeader;
    QPushButton* m_prevCueButton = nullptr;
    QPushButton* m_nextCueButton = nullptr;
    QLabel* m_contextLabel;
    QLabel* m_mappingStateLabel;
    QCheckBox* m_useCueMappingCheck;
    QPushButton* m_copyFromShowButton;
    QPushButton* m_clearCueMappingButton;

    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;

    // per-DCA summary of what fires for the current cue
    QGroupBox* m_dcaOverviewGroup = nullptr;
    QGridLayout* m_dcaOverviewLayout = nullptr;
    QVector<QLabel*> m_dcaOverviewTitles;
    QVector<QLabel*> m_dcaOverviewMembers;

    QGroupBox* m_channelGroup;
    QGridLayout* m_channelLayout;
    QVector<QComboBox*> m_channelCombos;
    QVector<QLabel*> m_channelLabels;
    QVector<QLineEdit*> m_channelNameEdits;

    QGroupBox* m_busGroup;
    QGridLayout* m_busLayout;
    QVector<QComboBox*> m_busCombos;
    QVector<QLabel*> m_busLabels;
    QVector<QLineEdit*> m_busNameEdits;

    QPushButton* m_syncButton;
    QPushButton* m_savePresetButton;
    QPushButton* m_loadPresetButton;
    QPushButton* m_clearAllButton;

    // actions
    QAction* m_syncFromMixerAction;
    QAction* m_savePresetAction;
    QAction* m_loadPresetAction;
    QAction* m_clearAllAction;

    int m_channelCount = 32;
    int m_busCount = 16;
    int m_dcaCount = 8;

    bool m_updatingUi = false;
};

} // namespace OpenMix
