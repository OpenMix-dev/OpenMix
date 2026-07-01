#include "FadeEngine.h"

#include <algorithm>

namespace OpenMix {

FadeEngine::FadeEngine(QObject* parent) : QObject(parent) {
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &FadeEngine::onTick);
}

double FadeEngine::ease(FadeCurve curve, double t) {
    t = std::clamp(t, 0.0, 1.0);
    switch (curve) {
    case FadeCurve::Linear:
        return t;
    case FadeCurve::EaseInOut:
        return t * t * (3.0 - 2.0 * t); // smoothstep
    case FadeCurve::EaseIn:
        return t * t;
    case FadeCurve::EaseOut:
        return 1.0 - (1.0 - t) * (1.0 - t);
    }
    return t;
}

void FadeEngine::setTickIntervalMs(int ms) { m_tickIntervalMs = std::max(1, ms); }

void FadeEngine::start(const QString& key, double from, double to, double durationMs,
                       FadeCurve curve, ApplyFn apply) {
    cancel(key); // restart any in-flight fade for this key

    if (!apply)
        return;

    if (durationMs <= 0.0) {
        apply(to);
        emit fadeCompleted(key);
        return;
    }

    apply(from);
    m_fades.append(Fade{key, from, to, durationMs, 0.0, curve, std::move(apply)});
    ensureTimerRunning();
}

void FadeEngine::cancel(const QString& key) {
    for (int i = 0; i < m_fades.size(); ++i) {
        if (m_fades[i].key == key) {
            m_fades.removeAt(i);
            break;
        }
    }
    if (m_fades.isEmpty())
        m_timer.stop();
}

void FadeEngine::cancelAll() {
    m_fades.clear();
    m_timer.stop();
}

void FadeEngine::ensureTimerRunning() {
    if (!m_timer.isActive()) {
        m_clock.restart();
        m_timer.start(m_tickIntervalMs);
    }
}

void FadeEngine::onTick() {
    double dt = static_cast<double>(m_clock.restart());
    advance(dt);
}

void FadeEngine::advance(double dtMs) {
    if (m_fades.isEmpty())
        return;

    // Compute this tick's values without invoking any callback yet, and copy
    // out each apply function. This makes advance() re-entrancy-safe: an apply
    // callback (or a fadeCompleted slot) may call start()/cancel() and realloc
    // m_fades without invalidating a reference we're holding mid-iteration.
    struct Step {
        QString key;
        double value;
        bool done;
        ApplyFn apply;
    };
    QVector<Step> steps;
    steps.reserve(m_fades.size());
    for (Fade& f : m_fades) {
        f.elapsedMs += dtMs;
        const double t = std::clamp(f.elapsedMs / f.durationMs, 0.0, 1.0);
        const double value = f.from + (f.to - f.from) * ease(f.curve, t);
        steps.append({f.key, value, t >= 1.0, f.apply});
    }

    // Remove completed fades before running callbacks so a callback sees a
    // consistent container.
    QStringList completedKeys;
    for (const Step& s : steps) {
        if (s.done) {
            completedKeys.append(s.key);
            cancel(s.key);
        }
    }

    // Now invoke callbacks from local copies; safe even if they mutate m_fades.
    for (const Step& s : steps)
        s.apply(s.value);

    if (m_fades.isEmpty())
        m_timer.stop();

    for (const QString& key : completedKeys)
        emit fadeCompleted(key);

    if (m_fades.isEmpty() && !completedKeys.isEmpty())
        emit allCompleted();
}

} // namespace OpenMix
