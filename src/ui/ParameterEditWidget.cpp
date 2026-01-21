#include "ParameterEditWidget.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>

namespace OpenMix {

ParameterEditWidget::ParameterEditWidget(const QString& path, QWidget* parent)
    : QWidget(parent), m_path(path) {
    determineControlType();
    setupUi();
}

void ParameterEditWidget::setupUi() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(8);

    // name label
    m_nameLabel = new QLabel(m_path, this);
    m_nameLabel->setMinimumWidth(120);
    m_nameLabel->setToolTip(m_path);
    layout->addWidget(m_nameLabel);

    if (m_isFaderType) {
        m_slider = new QSlider(Qt::Horizontal, this);
        m_slider->setRange(0, 1000);
        m_slider->setValue(0);
        m_slider->setMinimumWidth(150);
        layout->addWidget(m_slider, 1);

        m_valueLabel = new QLabel("0.000", this);
        m_valueLabel->setMinimumWidth(60);
        m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(m_valueLabel);

        m_checkbox = nullptr;

        connect(m_slider, &QSlider::valueChanged, this, &ParameterEditWidget::onSliderChanged);
    } else {
        m_checkbox = new QCheckBox(this);
        layout->addWidget(m_checkbox, 1);

        m_valueLabel = new QLabel("Off", this);
        m_valueLabel->setMinimumWidth(40);
        layout->addWidget(m_valueLabel);

        m_slider = nullptr;

        connect(m_checkbox, &QCheckBox::toggled, this, &ParameterEditWidget::onCheckboxToggled);
    }

    // revert button
    m_revertButton = new QPushButton(tr("Revert"), this);
    m_revertButton->setFixedWidth(60);
    m_revertButton->setEnabled(false);
    layout->addWidget(m_revertButton);

    connect(m_revertButton, &QPushButton::clicked, this, &ParameterEditWidget::onRevertClicked);
}

void ParameterEditWidget::determineControlType() {
    // determine fader or on/off param from path
    QString lowerPath = m_path.toLower();
    if (lowerPath.contains("/on") || lowerPath.contains("/mute") || lowerPath.contains("/solo") ||
        lowerPath.contains("/insert") || lowerPath.contains("/enable")) {
        m_isFaderType = false;
    } else {
        m_isFaderType = true;
    }
}

void ParameterEditWidget::setValue(const QVariant& value) {
    if (m_value == value) {
        return;
    }

    m_value = value;
    updateDisplay();
}

void ParameterEditWidget::setOriginalValue(const QVariant& value) {
    m_originalValue = value;
    if (!m_value.isValid()) {
        m_value = value;
    }
    updateDisplay();
}

void ParameterEditWidget::setState(ParameterEditState state) {
    if (m_state != state) {
        m_state = state;
        update(); // trigger repaint
    }
}

void ParameterEditWidget::setLabel(const QString& label) {
    m_nameLabel->setText(label);
    m_nameLabel->setToolTip(m_path + "\n" + label);
}

QString ParameterEditWidget::label() const { return m_nameLabel->text(); }

bool ParameterEditWidget::isModified() const { return m_value != m_originalValue; }

QSize ParameterEditWidget::sizeHint() const { return QSize(400, 30); }

QSize ParameterEditWidget::minimumSizeHint() const { return QSize(300, 24); }

void ParameterEditWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);

    // draw border based on state
    switch (m_state) {
    case ParameterEditState::Original:
        // no special border
        break;

    case ParameterEditState::Modified:
        painter.setPen(QPen(QColor(255, 200, 100), 2));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
        break;

    case ParameterEditState::Preview:
        painter.setPen(QPen(QColor(100, 150, 255), 2, Qt::DashLine));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
        break;
    }
}

void ParameterEditWidget::onSliderChanged(int value) {
    if (m_updatingUi) {
        return;
    }

    double floatValue = value / 1000.0;
    m_value = floatValue;

    m_valueLabel->setText(formatValue(m_value));

    // update state
    setState(isModified() ? ParameterEditState::Modified : ParameterEditState::Original);
    m_revertButton->setEnabled(isModified());

    emit valueChanged(m_path, m_value);
}

void ParameterEditWidget::onCheckboxToggled(bool checked) {
    if (m_updatingUi) {
        return;
    }

    m_value = checked ? 1 : 0;

    m_valueLabel->setText(checked ? tr("On") : tr("Off"));

    // update state
    setState(isModified() ? ParameterEditState::Modified : ParameterEditState::Original);
    m_revertButton->setEnabled(isModified());

    emit valueChanged(m_path, m_value);
}

void ParameterEditWidget::onRevertClicked() { emit revertRequested(m_path); }

void ParameterEditWidget::updateDisplay() {
    m_updatingUi = true;

    if (m_isFaderType && m_slider) {
        double floatValue = m_value.toDouble();
        m_slider->setValue(static_cast<int>(floatValue * 1000));
        m_valueLabel->setText(formatValue(m_value));
    } else if (!m_isFaderType && m_checkbox) {
        bool checked = m_value.toInt() != 0 || m_value.toBool();
        m_checkbox->setChecked(checked);
        m_valueLabel->setText(checked ? tr("On") : tr("Off"));
    }

    // update state
    setState(isModified() ? ParameterEditState::Modified : ParameterEditState::Original);
    m_revertButton->setEnabled(isModified());

    m_updatingUi = false;
}

QString ParameterEditWidget::formatValue(const QVariant& value) const {
    if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
        return QString::number(value.toDouble(), 'f', 3);
    } else if (value.typeId() == QMetaType::Int) {
        return QString::number(value.toInt());
    } else if (value.typeId() == QMetaType::Bool) {
        return value.toBool() ? tr("On") : tr("Off");
    }
    return value.toString();
}

} // namespace OpenMix
