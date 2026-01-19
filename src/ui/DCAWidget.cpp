#include "DCAWidget.h"
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

namespace StageBlend {

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
    nameFont.setPointSize(8);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);

    // level display
    m_levelLabel = new QLabel("-inf", this);
    m_levelLabel->setAlignment(Qt::AlignCenter);
    QFont levelFont;
    levelFont.setPointSize(7);
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
    layout->addWidget(m_levelLabel);
    layout->addWidget(m_faderSlider, 1, Qt::AlignHCenter);
    layout->addWidget(m_muteButton, 0, Qt::AlignHCenter);

    updateDisplay();
}

void DCAWidget::setLevel(float level) {
    m_level = qBound(0.0f, level, 1.0f);
    m_faderSlider->setValue(static_cast<int>(m_level * 1000));
    updateDisplay();
}

void DCAWidget::setMuted(bool muted) {
    m_muted = muted;
    m_muteButton->setChecked(muted);
    updateDisplay();
}

void DCAWidget::setName(const QString& name) {
    m_name = name;
    if (name.isEmpty()) {
        m_nameLabel->setText(QString("DCA %1").arg(m_dcaNumber));
    } else {
        m_nameLabel->setText(name);
    }
}

void DCAWidget::setActive(bool active) {
    if (m_active != active) {
        m_active = active;
        update();
    }
}

QSize DCAWidget::sizeHint() const { return QSize(50, 180); }

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

    // update mute button style
    if (m_muted) {
        m_muteButton->setStyleSheet(
            "QPushButton { background-color: #cc4444; color: white; font-weight: bold; }");
    } else {
        m_muteButton->setStyleSheet("QPushButton { background-color: #444444; color: #aaa; }");
    }
}

void DCAWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    if (m_active) {
        QPainter painter(this);
        painter.setPen(QPen(QColor(100, 200, 100), 2));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}

} // namespace StageBlend
