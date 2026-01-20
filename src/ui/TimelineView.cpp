#include "TimelineView.h"
#include "TimelineItem.h"
#include "app/Application.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include <QGraphicsScene>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

namespace OpenMix {

TimelineView::TimelineView(Application* app, QWidget* parent) : QGraphicsView(parent), m_app(app) {
    setupScene();

    // connect to cue list changes
    if (m_app && m_app->show()) {
        CueList* cueList = m_app->show()->cueList();
        connect(cueList, &CueList::cueAdded, this, &TimelineView::rebuild);
        connect(cueList, &CueList::cueRemoved, this, &TimelineView::rebuild);
        connect(cueList, &CueList::cueUpdated, this, &TimelineView::rebuild);
        connect(cueList, &CueList::cueMoved, this, &TimelineView::rebuild);
        connect(cueList, &CueList::listCleared, this, &TimelineView::rebuild);
    }

    // connect to playback engine
    if (m_app && m_app->playbackEngine()) {
        PlaybackEngine* engine = m_app->playbackEngine();
        connect(engine, &PlaybackEngine::currentCueChanged, this,
                &TimelineView::onCurrentCueChanged);
        connect(engine, &PlaybackEngine::fadeProgressChanged, this, &TimelineView::onFadeProgress);
    }

    rebuild();
}

void TimelineView::setupScene() {
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setDragMode(QGraphicsView::ScrollHandDrag);

    // create ruler background
    m_rulerBackground =
        m_scene->addRect(0, 0, 1000, RULER_HEIGHT, QPen(Qt::NoPen), QBrush(QColor(60, 60, 60)));
    m_rulerBackground->setZValue(100);

    // create playhead
    m_playhead =
        m_scene->addLine(0, 0, 0, RULER_HEIGHT + CUE_HEIGHT + CUE_MARGIN * 2, QPen(Qt::red, 2));
    m_playhead->setZValue(200);
    m_playhead->setVisible(false);
}

void TimelineView::rebuild() {
    // clear existing cue items
    for (TimelineCueItem* item : m_cueItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_cueItems.clear();

    // clear ruler marks
    for (QGraphicsTextItem* mark : m_rulerMarks) {
        m_scene->removeItem(mark);
        delete mark;
    }
    m_rulerMarks.clear();

    if (!m_app || !m_app->show())
        return;

    CueList* cueList = m_app->show()->cueList();
    if (!cueList)
        return;

    // calculate cumulative start times based on fade times + auto-follow delays
    double currentTime = 0.0;

    for (int i = 0; i < cueList->count(); ++i) {
        const Cue& cue = cueList->at(i);

        TimelineCueItem* item = new TimelineCueItem(cue, i);
        item->setTimeScale(m_timeScale);
        item->setStartTime(currentTime);
        item->setPos(item->pos().x(), RULER_HEIGHT + CUE_MARGIN);

        m_scene->addItem(item);
        m_cueItems.append(item);

        // calculate next cue's start time
        double duration = cue.fadeTime();
        if (duration < 1.0)
            duration = 1.0; // minimum display duration

        currentTime += duration;

        // add auto-follow delay
        if (cue.autoFollow()) {
            currentTime += cue.autoFollowDelay();
        }
    }

    updateTimeRuler();
    updateCuePositions();

    // highlight current cue
    if (m_app->playbackEngine()) {
        onCurrentCueChanged(m_app->playbackEngine()->currentCueIndex());
    }
}

void TimelineView::updateTimeRuler() {
    double totalDuration = calculateTotalDuration();
    double sceneWidth = totalDuration * m_timeScale + 200;

    // update ruler background
    m_rulerBackground->setRect(0, 0, sceneWidth, RULER_HEIGHT);

    // clear old marks
    for (QGraphicsTextItem* mark : m_rulerMarks) {
        m_scene->removeItem(mark);
        delete mark;
    }
    m_rulerMarks.clear();

    // add time marks
    double interval = 5.0; // 5 second intervals
    if (m_timeScale < 20)
        interval = 10.0;
    if (m_timeScale > 100)
        interval = 1.0;

    for (double t = 0; t <= totalDuration + interval; t += interval) {
        double x = t * m_timeScale;

        // add tick line
        QGraphicsLineItem* tick =
            m_scene->addLine(x, RULER_HEIGHT - 8, x, RULER_HEIGHT, QPen(QColor(150, 150, 150), 1));
        tick->setZValue(101);

        // add time label
        int minutes = static_cast<int>(t) / 60;
        int seconds = static_cast<int>(t) % 60;
        QString label = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));

        QGraphicsTextItem* text = m_scene->addText(label);
        text->setDefaultTextColor(QColor(200, 200, 200));
        QFont font;
        font.setPointSize(8);
        text->setFont(font);
        text->setPos(x - text->boundingRect().width() / 2, 2);
        text->setZValue(101);
        m_rulerMarks.append(text);
    }

    // update scene rect
    m_scene->setSceneRect(0, 0, sceneWidth, RULER_HEIGHT + CUE_HEIGHT + CUE_MARGIN * 2 + 10);
}

void TimelineView::updateCuePositions() {
    for (TimelineCueItem* item : m_cueItems) {
        item->setTimeScale(m_timeScale);
    }
}

double TimelineView::calculateTotalDuration() const {
    if (!m_app || !m_app->show())
        return 60.0;

    CueList* cueList = m_app->show()->cueList();
    if (!cueList || cueList->isEmpty())
        return 60.0;

    double total = 0.0;
    for (int i = 0; i < cueList->count(); ++i) {
        const Cue& cue = cueList->at(i);
        double duration = cue.fadeTime();
        if (duration < 1.0)
            duration = 1.0;
        total += duration;
        if (cue.autoFollow()) {
            total += cue.autoFollowDelay();
        }
    }

    return qMax(60.0, total);
}

void TimelineView::setTimeScale(double pixelsPerSecond) {
    m_timeScale = qBound(5.0, pixelsPerSecond, 200.0);
    rebuild();
}

void TimelineView::zoomIn() { setTimeScale(m_timeScale * 1.25); }

void TimelineView::zoomOut() { setTimeScale(m_timeScale / 1.25); }

void TimelineView::zoomToFit() {
    double duration = calculateTotalDuration();
    double viewWidth = viewport()->width() - 50;
    setTimeScale(viewWidth / duration);
}

void TimelineView::scrollToTime(double seconds) {
    double x = seconds * m_timeScale;
    horizontalScrollBar()->setValue(static_cast<int>(x - viewport()->width() / 2));
}

void TimelineView::scrollToCue(int index) {
    if (index >= 0 && index < m_cueItems.size()) {
        TimelineCueItem* item = m_cueItems[index];
        centerOn(item);
    }
}

void TimelineView::updatePlayhead(double currentTime) {
    m_currentTime = currentTime;
    double x = currentTime * m_timeScale;

    m_playhead->setLine(x, 0, x, RULER_HEIGHT + CUE_HEIGHT + CUE_MARGIN * 2);
    m_playhead->setVisible(true);

    emit timePositionChanged(currentTime);
}

void TimelineView::onCueStarted(int index) {
    if (index >= 0 && index < m_cueItems.size()) {
        for (TimelineCueItem* item : m_cueItems) {
            item->setActive(item->cueIndex() == index);
            item->setFadeProgress(0.0);
        }
        scrollToCue(index);
    }
}

void TimelineView::onFadeProgress(double progress) {
    if (m_currentCueIndex >= 0 && m_currentCueIndex < m_cueItems.size()) {
        m_cueItems[m_currentCueIndex]->setFadeProgress(progress);
    }
}

void TimelineView::onCurrentCueChanged(int index) {
    m_currentCueIndex = index;

    for (TimelineCueItem* item : m_cueItems) {
        item->setActive(item->cueIndex() == index);
        if (item->cueIndex() != index) {
            item->setFadeProgress(0.0);
        }
    }

    if (index >= 0 && index < m_cueItems.size()) {
        // update playhead position
        double startTime = m_cueItems[index]->startTime();
        updatePlayhead(startTime);
    }
}

void TimelineView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // zoom w/ ctrl+wheel
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void TimelineView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    updateTimeRuler();
}

void TimelineView::mousePressEvent(QMouseEvent* event) {
    QGraphicsView::mousePressEvent(event);

    // check if a cue item was clicked
    QGraphicsItem* item = itemAt(event->pos());
    TimelineCueItem* cueItem = dynamic_cast<TimelineCueItem*>(item);
    if (cueItem) {
        emit cueClicked(cueItem->cueIndex());
    }
}

} // namespace OpenMix
