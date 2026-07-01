#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <lo/lo.h>

namespace OpenMix {

// REAPER link for virtual sound check: on each cue fire, either drop a REAPER
// marker (record mode) or jump the REAPER playhead to that cue's marker
// (playback mode), so sections can be rehearsed against a recorded multitrack.
// Talks REAPER's OSC dialect; a live REAPER session is needed to exercise it.
class ReaperClient : public QObject {
    Q_OBJECT

  public:
    // REAPER's default OSC receive port.
    static constexpr int REAPER_DEFAULT_PORT = 8000;
    // REAPER action id: "Markers: Insert marker at current position".
    static constexpr int ACTION_INSERT_MARKER = 40157;

    // one dropped marker in the current sound-check session, plus its note.
    struct MarkerEntry {
        double cueNumber = 0.0;
        QString cueName;
        int index = 0;
        QString note;
    };

    explicit ReaperClient(QObject* parent = nullptr);
    ~ReaperClient() override;

    void setTarget(const QString& host, int port);
    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] int port() const { return m_port; }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    // record mode drops markers on fire; playback mode jumps to them.
    void setRecordMode(bool recording) { m_recordMode = recording; }
    [[nodiscard]] bool recordMode() const { return m_recordMode; }

    // marker pre-roll in seconds, applied when jumping (playback mode).
    void setPreRollSeconds(int seconds) { m_preRollSeconds = seconds < 0 ? 0 : seconds; }
    [[nodiscard]] int preRollSeconds() const { return m_preRollSeconds; }

    void loadFromSettings();
    void saveToSettings();

    // session markers dropped so far, in order, with their notes.
    [[nodiscard]] QList<MarkerEntry> markers() const { return m_markers; }
    void setMarkerNoteAt(int row, const QString& note);

  public slots:
    // route a fired cue: drop or jump a marker depending on the mode.
    void onCueFired(double cueNumber, const QString& name);
    // forget the session markers (e.g. on new REAPER session).
    void resetMarkers();

  signals:
    void markersChanged();

  signals:
    void sent(const QString& address);

  private:
    void rebuildAddress();
    void send(const QString& address);
    void sendString(const QString& address, const QString& value);
    void placeMarker(double cueNumber, const QString& name);
    void jumpToMarker(int index);

    lo_address m_address = nullptr;
    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = REAPER_DEFAULT_PORT;
    bool m_enabled = false;
    bool m_recordMode = false;
    int m_preRollSeconds = 0;

    QList<MarkerEntry> m_markers; // session markers, in fire order
    int m_nextMarker = 1;
};

} // namespace OpenMix
