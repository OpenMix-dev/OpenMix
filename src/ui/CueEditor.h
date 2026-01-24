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
class QVBoxLayout;

namespace OpenMix {

class Application;
class Cue;

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

  private:
    void setupUi();
    void createDCATargetingSection();
    void updateFromCue();
    void updateDCAOverridesUI();
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

    // DCA targeting widgets
    QGroupBox* m_dcaTargetingGroup;
    QCheckBox* m_targetAllDCAsCheck;
    QVector<QCheckBox*> m_dcaTargetChecks;

    // DCA overrides (mute/label per DCA)
    QGroupBox* m_dcaOverridesGroup;
    QScrollArea* m_dcaOverridesScroll;
    struct DCAOverrideWidgets {
        QCheckBox* enableMute;
        QCheckBox* muteValue;
        QCheckBox* enableLabel;
        QLineEdit* labelValue;
    };
    QVector<DCAOverrideWidgets> m_dcaOverrideWidgets;
};

} // namespace OpenMix
