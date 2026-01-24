#include "BubbleButton.h"
#include "theme/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyle>

namespace OpenMix {

BubbleButton::BubbleButton(const QString& icon, const QString& tooltip, QWidget* parent)
    : QPushButton(parent), m_iconText(icon) {
    setObjectName(Theme::ObjectNames::BubbleButton);
    setToolTip(tooltip);
    setCheckable(true);
    setFixedSize(36, 36);

    QFont f = font();
    f.setPointSize(16);
    setFont(f);

    setText(icon);
}

BubbleButton::BubbleButton(const QIcon& icon, const QString& tooltip, QWidget* parent)
    : QPushButton(parent) {
    setObjectName(Theme::ObjectNames::BubbleButton);
    setToolTip(tooltip);
    setCheckable(true);
    setFixedSize(36, 36);
    QPushButton::setIcon(icon);
    setIconSize(QSize(20, 20));
}

void BubbleButton::setBadge(const QString& text) {
    if (m_badgeText != text) {
        m_badgeText = text;
        update();
    }
}

void BubbleButton::setActive(bool active) {
    if (m_active != active) {
        m_active = active;
        setProperty(Theme::Properties::Active, active);
        style()->unpolish(this);
        style()->polish(this);
        emit activeChanged(active);
        update();
    }
}

void BubbleButton::setIcon(const QString& icon) {
    m_iconText = icon;
    setText(icon);
    update();
}

void BubbleButton::setButtonIcon(const QIcon& icon) {
    m_iconText.clear();
    setText(QString());
    QPushButton::setIcon(icon);
    setIconSize(QSize(20, 20));
    update();
}

void BubbleButton::paintEvent(QPaintEvent* event) {
    QPushButton::paintEvent(event);

    if (!m_badgeText.isEmpty()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont badgeFont = font();
        badgeFont.setPointSize(8);
        badgeFont.setBold(true);
        painter.setFont(badgeFont);

        QFontMetrics fm(badgeFont);
        int textWidth = fm.horizontalAdvance(m_badgeText);
        int badgeWidth = qMax(14, textWidth + 6);
        int badgeHeight = 14;

        int x = width() - badgeWidth - 2;
        int y = 2;

        QRectF badgeRect(x, y, badgeWidth, badgeHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::color(Theme::AccentRed));
        painter.drawRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);

        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, m_badgeText);
    }
}

} // namespace OpenMix
