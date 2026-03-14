#pragma once

#include <QAction>
#include <QComboBox>
#include <QKeyEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

class QCheckBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QScrollArea;

namespace OpenMix {

class NoScrollComboBox : public QComboBox {
    Q_OBJECT
  public:
    using QComboBox::QComboBox;

  protected:
    void wheelEvent(QWheelEvent* event) override { event->ignore(); }

    void showPopup() override {
        m_popupOpen = true;
        QComboBox::showPopup();
    }

    void hidePopup() override {
        QComboBox::hidePopup();
        QTimer::singleShot(100, this, [this]() { m_popupOpen = false; });
    }

    void keyPressEvent(QKeyEvent* event) override {
        int key = event->key();
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !m_popupOpen) {
            showPopup();
            event->accept();
        } else {
            QComboBox::keyPressEvent(event);
        }
    }

  private:
    bool m_popupOpen = false;
};

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

  private:
    void setupUi();
    void createChannelSection();
    void createBusSection();
    void updateDCAOptions();
    void populateFromMapping();
    void updateComboItemStates();
    void updateContextHeader();

    Application* m_app;
    DCAMapping* m_mapping;

    // cue-specific mapping
    Cue* m_currentCue = nullptr;
    bool m_showingCueMapping = false;

    // UI elements
    QWidget* m_contextHeader;
    QLabel* m_contextLabel;
    QCheckBox* m_useCueMappingCheck;
    QPushButton* m_copyFromShowButton;
    QPushButton* m_clearCueMappingButton;

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
