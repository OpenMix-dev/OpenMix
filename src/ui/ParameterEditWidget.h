#pragma once

#include <QVariant>
#include <QWidget>

class QSlider;
class QCheckBox;
class QLabel;
class QPushButton;

namespace OpenMix {

enum class ParameterEditState { Original, Modified, Preview };

class ParameterEditWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ParameterEditWidget(const QString& path, QWidget* parent = nullptr);

    QString path() const { return m_path; }

    void setValue(const QVariant& value);
    QVariant value() const { return m_value; }

    void setOriginalValue(const QVariant& value);
    QVariant originalValue() const { return m_originalValue; }

    void setState(ParameterEditState state);
    ParameterEditState state() const { return m_state; }

    void setLabel(const QString& label);
    QString label() const;

    bool isModified() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    void valueChanged(const QString& path, const QVariant& value);
    void revertRequested(const QString& path);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private slots:
    void onSliderChanged(int value);
    void onCheckboxToggled(bool checked);
    void onRevertClicked();

  private:
    void setupUi();
    void updateDisplay();
    void determineControlType();
    QString formatValue(const QVariant& value) const;

    QString m_path;
    QVariant m_value;
    QVariant m_originalValue;
    ParameterEditState m_state = ParameterEditState::Original;

    // widgets
    QLabel* m_nameLabel;
    QLabel* m_valueLabel;
    QSlider* m_slider;
    QCheckBox* m_checkbox;
    QPushButton* m_revertButton;

    bool m_isFaderType = true; // true for sliders, false for checkboxes
    bool m_updatingUi = false;
};

} // namespace OpenMix
