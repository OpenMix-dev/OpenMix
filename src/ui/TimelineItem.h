#pragma once

#include "core/Cue.h"
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>

namespace StageBlend {

class TimelineCueItem : public QGraphicsRectItem {
  public:
    TimelineCueItem(const Cue& cue, int index, QGraphicsItem* parent = nullptr);

    int cueIndex() const { return m_index; }
    QString cueId() const { return m_cueId; }

    void setTimeScale(double pixelsPerSecond);
    double timeScale() const { return m_timeScale; }

    void setActive(bool active);
    bool isActive() const { return m_isActive; }

    void setFadeProgress(double progress);
    double fadeProgress() const { return m_fadeProgress; }

    void setStartTime(double seconds);
    double startTime() const { return m_startTime; }

    void updateFromCue(const Cue& cue);

  protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

  private:
    void updateGeometry();
    QColor typeColor() const;

    int m_index;
    QString m_cueId;
    QString m_name;
    double m_cueNumber;
    CueType m_cueType;
    double m_fadeTime;
    double m_startTime = 0.0;
    double m_timeScale = 50.0;
    bool m_isActive = false;
    bool m_isHovered = false;
    double m_fadeProgress = 0.0;

    QGraphicsTextItem* m_label = nullptr;

    static constexpr double MIN_WIDTH = 30.0;
};

} // namespace StageBlend
