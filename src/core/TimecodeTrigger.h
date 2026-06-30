#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

namespace OpenMix {

// One timecode -> cue association: when playback timecode reaches hh:mm:ss:ff,
// fire the cue with this number.
struct TimecodeTrigger {
    QString id;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    double cueNumber = 0.0;
    bool enabled = true;

    QJsonObject toJson() const;
    [[nodiscard]] static TimecodeTrigger fromJson(const QJsonObject& json);
};

// Registry of timecode triggers. Fed incoming timecode via onTimecode() (wire it
// to MidiInputManager::timecodeChanged); emits triggerFired() with the target cue
// number as timecode crosses each trigger's point, so the caller can route it to
// PlaybackEngine (goToNumber + go).
class TimecodeTriggerList : public QObject {
    Q_OBJECT

  public:
    explicit TimecodeTriggerList(QObject* parent = nullptr);

    QString addTrigger(int h, int m, int s, int f, double cueNumber);
    void addTrigger(const TimecodeTrigger& trigger);
    bool removeTrigger(const QString& id);
    void setTriggerEnabled(const QString& id, bool enabled);
    void setTriggers(const QList<TimecodeTrigger>& triggers);
    void clear();

    [[nodiscard]] QList<TimecodeTrigger> triggers() const { return m_triggers; }
    [[nodiscard]] int count() const { return m_triggers.size(); }
    [[nodiscard]] bool isEmpty() const { return m_triggers.isEmpty(); }

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  public slots:
    // feed current playback timecode; fires any trigger whose point is crossed
    void onTimecode(int hours, int minutes, int seconds, int frames);
    // re-arm all triggers (e.g. when transport stops / relocates)
    void resetArming();

  signals:
    void triggerFired(double cueNumber, const QString& triggerId);
    void triggerAdded(const QString& id);
    void triggerRemoved(const QString& id);
    void triggersCleared();

  private:
    // monotonic ordering key; FPS_BASE comfortably exceeds any real frame count so
    // the key orders correctly across 24/25/29.97/30 fps without knowing the rate.
    static constexpr long FPS_BASE = 100;
    static long order(int h, int m, int s, int f) {
        return ((static_cast<long>(h) * 60 + m) * 60 + s) * FPS_BASE + f;
    }

    QList<TimecodeTrigger> m_triggers;
    QSet<QString> m_fired; // triggers already fired since last crossing/re-arm
    long m_lastOrder = -1;
};

} // namespace OpenMix
