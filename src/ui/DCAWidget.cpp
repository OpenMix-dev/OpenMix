#include "DCAWidget.h"
#include "theme/Theme.h"
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace OpenMix {

DCAWidget::DCAWidget(int dcaNumber, QWidget* parent) : QWidget(parent), m_dcaNumber(dcaNumber) {
    setupUi();
}

void DCAWidget::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // name label at top
    m_nameLabel = new QLabel(QString("DCA %1").arg(m_dcaNumber), this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    QFont nameFont;
    nameFont.setPointSize(9);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->installEventFilter(this);

    // name edit
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setAlignment(Qt::AlignCenter);
    m_nameEdit->setFont(nameFont);
    m_nameEdit->setVisible(false);
    m_nameEdit->installEventFilter(this);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &DCAWidget::finishLabelEdit);

    // level display
    m_levelLabel = new QLabel("-inf", this);
    m_levelLabel->setAlignment(Qt::AlignCenter);
    QFont levelFont;
    levelFont.setPointSize(9);
    m_levelLabel->setFont(levelFont);

    // fader slider (vertical)
    m_faderSlider = new QSlider(Qt::Vertical, this);
    m_faderSlider->setRange(0, 1000);
    m_faderSlider->setValue(0);
    m_faderSlider->setEnabled(false); // read-only display
    m_faderSlider->setMinimumHeight(100);

    // mute button
    m_muteButton = new QPushButton("M", this);
    m_muteButton->setCheckable(true);
    m_muteButton->setFixedSize(24, 24);
    m_muteButton->setEnabled(false); // read-only display

    // layout
    layout->addWidget(m_nameLabel);
    layout->addWidget(m_nameEdit);
    layout->addWidget(m_levelLabel);
    layout->addWidget(m_faderSlider, 1, Qt::AlignHCenter);
    layout->addWidget(m_muteButton, 0, Qt::AlignHCenter);

    updateDisplay();
}

void DCAWidget::setLevel(float level) {
    m_level = std::clamp(level, 0.0f, 1.0f);
    m_faderSlider->setValue(static_cast<int>(m_level * 1000));
    updateDisplay();
}

void DCAWidget::setMuted(bool muted) {
    m_muted = muted;
    m_muteButton->setChecked(muted);
    updateDisplay();
}

void DCAWidget::setMixerName(const QString& name) {
    m_mixerName = name;
    updateNameDisplay();
}

void DCAWidget::setCueLabel(const QString& label) {
    m_cueLabel = label;
    updateNameDisplay();
}

QString DCAWidget::displayName() const {
    if (!m_cueLabel.isEmpty()) {
        return m_cueLabel;
    }
    if (!m_mixerName.isEmpty()) {
        return m_mixerName;
    }
    return QString("DCA %1").arg(m_dcaNumber);
}

void DCAWidget::setLabelEditEnabled(bool enabled) {
    m_labelEditEnabled = enabled;
    if (!enabled && m_nameEdit->isVisible()) {
        cancelLabelEdit();
    }
    m_nameLabel->setCursor(enabled ? Qt::IBeamCursor : Qt::ArrowCursor);
}

void DCAWidget::updateNameDisplay() { m_nameLabel->setText(displayName()); }

void DCAWidget::startLabelEdit() {
    if (!m_labelEditEnabled) {
        return;
    }

    m_nameEdit->setText(m_cueLabel.isEmpty() ? m_mixerName : m_cueLabel);
    m_nameLabel->setVisible(false);
    m_nameEdit->setVisible(true);
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void DCAWidget::finishLabelEdit() {
    if (!m_nameEdit->isVisible()) {
        return;
    }

    QString newLabel = m_nameEdit->text().trimmed();
    m_nameEdit->setVisible(false);
    m_nameLabel->setVisible(true);

    // emit signal if label changed
    if (newLabel != m_cueLabel) {
        emit labelEdited(m_dcaNumber, newLabel);
    }
}

void DCAWidget::cancelLabelEdit() {
    m_nameEdit->setVisible(false);
    m_nameLabel->setVisible(true);
}

bool DCAWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_nameLabel && event->type() == QEvent::MouseButtonDblClick) {
        if (m_labelEditEnabled) {
            startLabelEdit();
            return true;
        }
    } else if (obj == m_nameEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            cancelLabelEdit();
            return true;
        } else if (keyEvent->key() == Qt::Key_Tab) {
            finishLabelEdit();
            emit tabToNextRequested(m_dcaNumber);
            return true;
        } else if (keyEvent->key() == Qt::Key_Backtab) {
            finishLabelEdit();
            emit tabToPreviousRequested(m_dcaNumber);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DCAWidget::setActive(bool active) {
    if (m_active != active) {
        m_active = active;
        update();
    }
}

void DCAWidget::setEditMode(bool editMode) {
    if (m_editMode != editMode) {
        m_editMode = editMode;
        if (editMode) {
            m_originalLevel = m_level;
        }
        update();
    }
}

void DCAWidget::setPreviewMode(bool preview) {
    if (m_previewMode != preview) {
        m_previewMode = preview;
        update();
    }
}

void DCAWidget::setOriginalLevel(float level) {
    m_originalLevel = std::clamp(level, 0.0f, 1.0f);
    update();
}

QSize DCAWidget::sizeHint() const { return QSize(56, 184); }

QSize DCAWidget::minimumSizeHint() const { return QSize(40, 120); }

QString DCAWidget::levelToDb(float level) const {
    if (level <= 0.0f) {
        return "-inf";
    }

    // x32 fader curve approximation
    // 0.0 = -inf, 0.75 = 0dB, 1.0 = +10dB
    double db;
    if (level <= 0.75f) {
        // -90dB to 0dB range
        db = (level / 0.75f) * 90.0 - 90.0;
    } else {
        // 0dB to +10dB range
        db = ((level - 0.75f) / 0.25f) * 10.0;
    }

    if (db <= -60.0) {
        return "-inf";
    }

    return QString("%1").arg(db, 0, 'f', 1);
}

void DCAWidget::updateDisplay() {
    m_levelLabel->setText(levelToDb(m_level) + " dB");

    // muted = red, unmuted = the shared inert look (not force-unmute green)
    m_muteButton->setStyleSheet(
        Theme::muteButtonStyle(m_muted ? std::optional<bool>(true) : std::nullopt));
}

void DCAWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);

    if (m_editMode) {
        // draw edit mode border using theme colors
        if (m_previewMode) {
            painter.setPen(QPen(Theme::color(Theme::Colors::AccentBlue), 2, Qt::DashLine));
        } else {
            painter.setPen(QPen(Theme::color(Theme::Colors::AccentAmber), 2));
        }
        painter.drawRect(rect().adjusted(1, 1, -1, -1));

        // draw original level indicator if level differs
        if (qAbs(m_level - m_originalLevel) > 0.01f && m_faderSlider) {
            int sliderY = m_faderSlider->y();
            int sliderHeight = m_faderSlider->height();
            int sliderX = m_faderSlider->x();
            int sliderWidth = m_faderSlider->width();

            // calculate pos for original level marker
            int originalY =
                sliderY + sliderHeight - static_cast<int>(m_originalLevel * sliderHeight);

            painter.setPen(QPen(Theme::color(Theme::Colors::AccentRed), 2));
            painter.drawLine(sliderX - 4, originalY, sliderX + sliderWidth + 4, originalY);
        }
    } else if (m_active) {
        painter.setPen(QPen(Theme::color(Theme::Colors::AccentGreen), 2));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}

} // namespace OpenMix
