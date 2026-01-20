#pragma once

#include <QGraphicsView>
#include <QVector>

namespace OpenMix {

class Application;
class CueList;
class TimelineCueItem;

class TimelineView : public QGraphicsView {
    Q_OBJECT

  public:
    explicit TimelineView(Application* app, QWidget* parent = nullptr);

    // time scale (pixels per second)
    void setTimeScale(double pixelsPerSecond);
    double timeScale() const { return m_timeScale; }

    // zoom controls
    void zoomIn();
    void zoomOut();
    void zoomToFit();

    // scroll to time position
    void scrollToTime(double seconds);
    void scrollToCue(int index);

  public slots:
    void rebuild();
    void updatePlayhead(double currentTime);
    void onCueStarted(int index);
    void onFadeProgress(double progress);
    void onCurrentCueChanged(int index);

  signals:
    void cueClicked(int index);
    void timePositionChanged(double seconds);

  protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

  private:
    void setupScene();
    void updateTimeRuler();
    void updateCuePositions();
    double calculateTotalDuration() const;

    Application* m_app;
    QGraphicsScene* m_scene;

    double m_timeScale = 50.0; // pixels per second
    double m_currentTime = 0.0;
    int m_currentCueIndex = -1;

    // scene items
    QGraphicsLineItem* m_playhead = nullptr;
    QGraphicsRectItem* m_rulerBackground = nullptr;
    QVector<QGraphicsTextItem*> m_rulerMarks;
    QVector<TimelineCueItem*> m_cueItems;

    static constexpr int RULER_HEIGHT = 30;
    static constexpr int CUE_HEIGHT = 40;
    static constexpr int CUE_MARGIN = 5;
};

} // namespace OpenMix
