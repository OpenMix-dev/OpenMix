#include "TimelineItem.h"
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace StageBlend {

TimelineCueItem::TimelineCueItem(const Cue& cue, int index, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), m_index(index), m_cueId(cue.id()), m_name(cue.name()),
      m_cueNumber(cue.number()), m_cueType(cue.type()), m_fadeTime(cue.fadeTime()) {
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable);

    // create label
    m_label = new QGraphicsTextItem(this);
    m_label->setDefaultTextColor(Qt::white);
    QFont font;
    font.setPointSize(8);
    m_label->setFont(font);

    updateGeometry();
}

void TimelineCueItem::setTimeScale(double pixelsPerSecond) {
    if (m_timeScale != pixelsPerSecond) {
        m_timeScale = pixelsPerSecond;
        updateGeometry();
    }
}

void TimelineCueItem::setActive(bool active) {
    if (m_isActive != active) {
        m_isActive = active;
        update();
    }
}

void TimelineCueItem::setFadeProgress(double progress) {
    if (m_fadeProgress != progress) {
        m_fadeProgress = qBound(0.0, progress, 1.0);
        update();
    }
}

void TimelineCueItem::setStartTime(double seconds) {
    if (m_startTime != seconds) {
        m_startTime = seconds;
        updateGeometry();
    }
}

void TimelineCueItem::updateFromCue(const Cue& cue) {
    m_name = cue.name();
    m_cueNumber = cue.number();
    m_cueType = cue.type();
    m_fadeTime = cue.fadeTime();
    updateGeometry();
}

void TimelineCueItem::updateGeometry() {
    double width = m_fadeTime * m_timeScale;
    if (width < MIN_WIDTH)
        width = MIN_WIDTH;

    double x = m_startTime * m_timeScale;

    setRect(0, 0, width, 35);
    setPos(x, 0);

    // update label
    QString labelText = QString("%1: %2").arg(m_cueNumber, 0, 'f', 1).arg(m_name);
    m_label->setPlainText(labelText);
    m_label->setPos(4, 2);

    // clip label to item width
    QRectF labelBounds = m_label->boundingRect();
    if (labelBounds.width() > width - 8) {
        // truncate text
        QString truncated = labelText;
        while (truncated.length() > 3 && m_label->boundingRect().width() > width - 12) {
            truncated.chop(1);
            m_label->setPlainText(truncated + "...");
        }
    }
}

QColor TimelineCueItem::typeColor() const {
    switch (m_cueType) {
    case CueType::Snapshot:
        return QColor(70, 130, 180); // steel blue
    case CueType::Fade:
        return QColor(60, 160, 100); // green
    case CueType::Stop:
        return QColor(180, 80, 80); // red
    case CueType::GoTo:
        return QColor(140, 100, 180); // purple
    case CueType::Wait:
        return QColor(180, 160, 80); // yellow
    case CueType::Macro:
        return QColor(180, 120, 60); // orange
    default:
        return QColor(128, 128, 128); // gray
    }
}

void TimelineCueItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    QRectF r = rect();
    QColor baseColor = typeColor();

    // highlight if active or hovered
    if (m_isActive) {
        baseColor = baseColor.lighter(120);
    } else if (m_isHovered) {
        baseColor = baseColor.lighter(110);
    }

    // draw background
    painter->setBrush(baseColor);
    painter->setPen(QPen(baseColor.darker(120), 1));
    painter->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

    // draw fade progress overlay if active
    if (m_isActive && m_fadeProgress > 0 && m_fadeTime > 0) {
        QRectF progressRect = r;
        progressRect.setWidth(r.width() * m_fadeProgress);

        painter->setBrush(QColor(255, 255, 255, 60));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(progressRect.adjusted(1, 1, 0, -1), 3, 3);
    }

    // draw selection border
    if (isSelected()) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::white, 2));
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
    }
}

void TimelineCueItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    m_isHovered = true;
    setCursor(Qt::PointingHandCursor);

    // show tooltip
    QString tip = QString("Cue %1: %2\nType: %3")
                      .arg(m_cueNumber, 0, 'f', 1)
                      .arg(m_name)
                      .arg(cueTypeToString(m_cueType));

    if (m_fadeTime > 0) {
        tip += QString("\nFade: %1s").arg(m_fadeTime, 0, 'f', 1);
    }

    setToolTip(tip);
    update();

    QGraphicsRectItem::hoverEnterEvent(event);
}

void TimelineCueItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    m_isHovered = false;
    unsetCursor();
    update();

    QGraphicsRectItem::hoverLeaveEvent(event);
}

void TimelineCueItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // selection will be handled by the view
    }
    QGraphicsRectItem::mousePressEvent(event);
}

} // namespace StageBlend
