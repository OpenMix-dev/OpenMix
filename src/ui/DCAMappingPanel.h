#pragma once

#include <QAction>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QScrollArea;

namespace OpenMix {

class Application;
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

  private slots:
    void onChannelDCAChanged(int channel, int dcaIndex);
    void onBusDCAChanged(int bus, int dcaIndex);
    void onMixerConnected();
    void onMixerDisconnected();

  private:
    void setupUi();
    void createChannelSection();
    void createBusSection();
    void updateDCAOptions();
    void populateFromMapping();
    void updateComboItemStates();

    Application* m_app;
    DCAMapping* m_mapping;

    // UI elements
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;

    QGroupBox* m_channelGroup;
    QGridLayout* m_channelLayout;
    QVector<QComboBox*> m_channelCombos;
    QVector<QLabel*> m_channelLabels;

    QGroupBox* m_busGroup;
    QGridLayout* m_busLayout;
    QVector<QComboBox*> m_busCombos;
    QVector<QLabel*> m_busLabels;

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
