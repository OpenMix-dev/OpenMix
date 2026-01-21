#pragma once

#include <QWidget>

class QSlider;
class QPushButton;
class QLabel;

namespace OpenMix {

class DCAWidget : public QWidget {
    Q_OBJECT

  public:
    explicit DCAWidget(int dcaNumber, QWidget* parent = nullptr);

    int dcaNumber() const { return m_dcaNumber; }

    void setLevel(float level);
    float level() const { return m_level; }

    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

    void setName(const QString& name);
    QString name() const { return m_name; }

    void setActive(bool active);
    bool isActive() const { return m_active; }

    // edit mode visualization
    void setEditMode(bool editMode);
    bool isEditMode() const { return m_editMode; }

    void setPreviewMode(bool preview);
    bool isPreviewMode() const { return m_previewMode; }

    void setOriginalLevel(float level);
    float originalLevel() const { return m_originalLevel; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    void levelChanged(int dcaNumber, float level);
    void muteToggled(int dcaNumber, bool muted);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    void setupUi();
    void updateDisplay();
    QString levelToDb(float level) const;

    int m_dcaNumber;
    float m_level = 0.0f;
    bool m_muted = false;
    QString m_name;
    bool m_active = false;
    bool m_editMode = false;
    bool m_previewMode = false;
    float m_originalLevel = 0.0f;

    QSlider* m_faderSlider;
    QPushButton* m_muteButton;
    QLabel* m_nameLabel;
    QLabel* m_levelLabel;
};

} // namespace OpenMix
