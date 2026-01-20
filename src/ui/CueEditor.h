#pragma once

#include <QWidget>

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QTextEdit;
class QCheckBox;
class QPushButton;
class QFormLayout;

namespace OpenMix {

class Application;
class Cue;

class CueEditor : public QWidget {
    Q_OBJECT

  public:
    explicit CueEditor(Application* app, QWidget* parent = nullptr);

    bool hasFocus() const;

  public slots:
    void setCue(int index);

  signals:
    void cueModified();

  private slots:
    void onNumberChanged(double value);
    void onNameChanged(const QString& text);
    void onTypeChanged(int index);
    void onFadeTimeChanged(double value);
    void onAutoFollowChanged(bool checked);
    void onAutoFollowDelayChanged(double value);
    void onNotesChanged();
    void onCaptureSnapshot();

  private:
    void setupUi();
    void updateFromCue();
    void setEnabled(bool enabled);
    Cue* currentCue();

    Application* m_app;
    int m_currentIndex = -1;
    bool m_updatingUi = false;

    // widgets
    QDoubleSpinBox* m_numberSpin;
    QLineEdit* m_nameEdit;
    QComboBox* m_typeCombo;
    QDoubleSpinBox* m_fadeTimeSpin;
    QCheckBox* m_autoFollowCheck;
    QDoubleSpinBox* m_autoFollowDelaySpin;
    QTextEdit* m_notesEdit;
    QPushButton* m_captureButton;
};

} // namespace OpenMix
