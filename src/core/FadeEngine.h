#pragma once

#include "FadeCurve.h"
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <functional>

namespace OpenMix {

// Drives time-based parameter fades. Each fade interpolates a value from->to over
// a duration with an easing curve, calling an apply callback every tick. The core
// stepping lives in advance(dtMs) so it can be unit-tested deterministically
// without spinning an event loop; a QTimer feeds real elapsed time in production.
class FadeEngine : public QObject {
    Q_OBJECT

  public:
    using ApplyFn = std::function<void(double)>;

    explicit FadeEngine(QObject* parent = nullptr);

    // start (or restart, if key already fading) a fade. durationMs <= 0 applies
    // the target immediately. The 'from' value is applied right away.
    void start(const QString& key, double from, double to, double durationMs, FadeCurve curve,
               ApplyFn apply);

    void cancel(const QString& key);
    void cancelAll();

    [[nodiscard]] bool isActive() const { return !m_fades.isEmpty(); }
    [[nodiscard]] int activeCount() const { return m_fades.size(); }

    void setTickIntervalMs(int ms);

    // advance every active fade by dtMs, applying interpolated values. Completed
    // fades apply their exact target, emit fadeCompleted, and are removed.
    void advance(double dtMs);

    static double ease(FadeCurve curve, double t);

  signals:
    void fadeCompleted(const QString& key);
    void allCompleted();

  private slots:
    void onTick();

  private:
    struct Fade {
        QString key;
        double from;
        double to;
        double durationMs;
        double elapsedMs;
        FadeCurve curve;
        ApplyFn apply;
    };

    void ensureTimerRunning();

    QVector<Fade> m_fades;
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_tickIntervalMs = 20;
};

} // namespace OpenMix
