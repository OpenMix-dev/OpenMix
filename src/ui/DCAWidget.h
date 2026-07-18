#pragma once

#include "core/LevelDb.h"

#include <QWidget>

class QSlider;
class QPushButton;
class QLabel;
class QLineEdit;

namespace OpenMix {

class DCAWidget : public QWidget {
    Q_OBJECT

  public:
    explicit DCAWidget(int dcaNumber, QWidget* parent = nullptr);

    int dcaNumber() const { return m_dcaNumber; }

    void setLevelDb(float dB);
    float levelDb() const { return m_levelDb; }

    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

    // mixer hardware name
    void setMixerName(const QString& name);
    QString mixerName() const { return m_mixerName; }

    // cue-specific label
    void setCueLabel(const QString& label);
    QString cueLabel() const { return m_cueLabel; }

    // cueLabel > mixerName > "DCA N"
    QString displayName() const;

    // enable inline label editing during live edits
    void setLabelEditEnabled(bool enabled);
    bool isLabelEditEnabled() const { return m_labelEditEnabled; }

    void setActive(bool active);
    bool isActive() const { return m_active; }

    // edit mode visualization
    void setEditMode(bool editMode);
    bool isEditMode() const { return m_editMode; }

    void setPreviewMode(bool preview);
    bool isPreviewMode() const { return m_previewMode; }

    void setOriginalLevel(float level);
    float originalLevel() const { return m_originalLevelDb; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // start inline label editing (called externally for tab navigation)
    void startLabelEdit();

  signals:
    void levelChanged(int dcaNumber, float level);
    void muteToggled(int dcaNumber, bool muted);
    void labelEdited(int dcaNumber, const QString& newLabel);
    void tabToNextRequested(int dcaNumber);
    void tabToPreviousRequested(int dcaNumber);

  protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void setupUi();
    void updateDisplay();
    void updateNameDisplay();
    void finishLabelEdit();
    void cancelLabelEdit();
    QString levelText(float dB) const;
    static int sliderPosition(float dB);

    int m_dcaNumber;
    float m_levelDb = NEG_INF_DB;
    bool m_muted = false;
    QString m_mixerName;
    QString m_cueLabel;
    bool m_active = false;
    bool m_editMode = false;
    bool m_previewMode = false;
    bool m_labelEditEnabled = false;
    float m_originalLevelDb = 0.0f;

    QSlider* m_faderSlider;
    QPushButton* m_muteButton;
    QLabel* m_nameLabel;
    QLineEdit* m_nameEdit;
    QLabel* m_levelLabel;
};

} // namespace OpenMix
