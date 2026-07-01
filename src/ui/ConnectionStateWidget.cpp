#include "ConnectionStateWidget.h"
#include "theme/Theme.h"
#include <QPainter>
#include <QPainterPath>

namespace OpenMix {

ConnectionStateWidget::ConnectionStateWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(24);

    m_blinkTimer.setInterval(500);
    connect(&m_blinkTimer, &QTimer::timeout, this, &ConnectionStateWidget::onBlinkTimer);
}

void ConnectionStateWidget::setState(ConnectionState state) {
    if (m_state != state) {
        m_state = state;

        // start/stop blink timer for transitional states
        if (state == ConnectionState::Connecting || state == ConnectionState::Reconnecting) {
            m_blinkTimer.start();
        } else {
            m_blinkTimer.stop();
            m_blinkState = false;
        }

        update();
    }
}

void ConnectionStateWidget::setLatency(int ms) {
    if (m_latencyMs != ms) {
        m_latencyMs = ms;
        update();
    }
}

QSize ConnectionStateWidget::sizeHint() const { return QSize(200, 28); }

QSize ConnectionStateWidget::minimumSizeHint() const { return QSize(100, 24); }

void ConnectionStateWidget::paintEvent([[maybe_unused]] QPaintEvent* event) {

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int circleSize = 16;
    int margin = 4;
    int textX = margin + circleSize + 8;

    // draw status circle
    QColor color = stateColor();

    // for blinking states, fade the color
    if ((m_state == ConnectionState::Connecting || m_state == ConnectionState::Reconnecting) &&
        m_blinkState) {
        color = color.lighter(150);
    }

    painter.setBrush(color);
    painter.setPen(Qt::NoPen);

    int circleY = (height() - circleSize) / 2;
    painter.drawEllipse(margin, circleY, circleSize, circleSize);

    // draw border around circle
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color.darker(120), 1));
    painter.drawEllipse(margin, circleY, circleSize, circleSize);

    // draw text
    painter.setPen(palette().text().color());
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);

    QString text = stateText();

    // add latency for connected state
    if (m_state == ConnectionState::Connected && m_latencyMs > 0) {
        text += QString(" (%1ms)").arg(m_latencyMs);
    }

    QRect textRect(textX, 0, width() - textX - margin, height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
}

QColor ConnectionStateWidget::stateColor() const {
    switch (m_state) {
    case ConnectionState::Disconnected:
        return Theme::color(Theme::Colors::AccentRed);
    case ConnectionState::Connecting:
        return Theme::color(Theme::Colors::AccentAmber);
    case ConnectionState::Connected:
        return Theme::color(Theme::Colors::AccentGreen);
    case ConnectionState::Reconnecting:
        return Theme::color(Theme::Colors::AccentAmberHover);
    default:
        return Theme::color(Theme::Colors::TextTertiary);
    }
}

QString ConnectionStateWidget::stateText() const {
    switch (m_state) {
    case ConnectionState::Disconnected:
        return tr("Disconnected");
    case ConnectionState::Connecting:
        return tr("Connecting...");
    case ConnectionState::Connected:
        return tr("Connected");
    case ConnectionState::Reconnecting:
        return tr("Reconnecting...");
    default:
        return tr("Unknown");
    }
}

void ConnectionStateWidget::onBlinkTimer() {
    m_blinkState = !m_blinkState;
    update();
}

} // namespace OpenMix
