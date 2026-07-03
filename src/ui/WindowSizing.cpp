#include "WindowSizing.h"

#include <QEvent>
#include <QHeaderView>
#include <QListView>
#include <QPointer>
#include <QScreen>
#include <QScrollArea>
#include <QTableView>
#include <QTimer>
#include <QTreeView>

namespace OpenMix {

namespace {

// content width a scroll surface needs to show everything without a
// horizontal scroll bar, or 0 for surfaces that wrap (text edits) or
// that we can't measure
int requiredViewportWidth(QAbstractScrollArea* area) {
    // header lengths and column hints are viewport-content widths; the
    // vertical header lives outside the viewport and must not be counted
    if (auto* table = qobject_cast<QTableView*>(area))
        return table->horizontalHeader()->length();
    if (auto* tree = qobject_cast<QTreeView*>(area))
        return tree->header()->length();
    if (auto* list = qobject_cast<QListView*>(area))
        return list->sizeHintForColumn(0);
    if (auto* scroll = qobject_cast<QScrollArea*>(area))
        return scroll->widget() ? scroll->widget()->sizeHint().width() : 0;
    return 0;
}

// one measure-and-grow pass; true if the window changed size
bool widenPass(QWidget* window) {
    int deficit = 0;
    const auto areas = window->findChildren<QAbstractScrollArea*>();
    for (QAbstractScrollArea* area : areas) {
        if (!area->isVisibleTo(window))
            continue;
        if (area->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff)
            continue;
        const int required = requiredViewportWidth(area);
        if (required <= 0)
            continue;
        deficit = qMax(deficit, required - area->viewport()->width());
    }
    if (deficit <= 0)
        return false;

    const QScreen* screen = window->screen();
    if (!screen)
        return false;
    const QRect avail = screen->availableGeometry();
    const int maxWidth = avail.width() - 32;
    const int newWidth = qMin(window->width() + deficit + 2, maxWidth);
    if (newWidth <= window->width())
        return false;

    window->resize(newWidth, window->height());
    // keep the grown window on screen
    if (window->frameGeometry().right() > avail.right())
        window->move(qMax(avail.left(), avail.right() - window->frameGeometry().width()),
                     window->y());
    return true;
}

// re-widens its window after every Show, once layout has settled; passes are
// repeated because splitters/layouts redistribute the added width
class ShowWidener : public QObject {
public:
    explicit ShowWidener(QWidget* window) : QObject(window), m_window(window) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_window && event->type() == QEvent::Show)
            schedulePass(MaxPasses);
        return QObject::eventFilter(watched, event);
    }

private:
    static constexpr int MaxPasses = 3;

    void schedulePass(int remaining) {
        QPointer<QWidget> window = m_window;
        QTimer::singleShot(0, this, [this, window, remaining] {
            if (!window || !window->isVisible())
                return;
            if (widenPass(window) && remaining > 1)
                schedulePass(remaining - 1);
        });
    }

    QWidget* m_window;
};

} // namespace

namespace WindowSizing {

void widenToContents(QWidget* window) {
    for (int pass = 0; pass < 3; ++pass)
        if (!widenPass(window))
            break;
}

void widenOnShow(QWidget* window) {
    auto* widener = new ShowWidener(window);
    window->installEventFilter(widener);
}

} // namespace WindowSizing

} // namespace OpenMix
