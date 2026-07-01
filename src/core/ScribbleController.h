#pragma once

#include "ChannelMonitor.h" // ChannelState
#include <QObject>
#include <QSet>
#include <QString>

namespace OpenMix {

class MixerProtocol;
class ActorProfileLibrary;
class CueList;
class DCAMapping;
class Cue;

// Drives console scribble strips from show data. It writes each channel's
// assigned actor name to that channel's strip, writes the current cue number to
// a configurable strip, and colors strips from ChannelMonitor silence/clip
// state. All pushes go through MixerProtocol::setChannelName / setChannelColor,
// which are no-ops on drivers that cannot address scribble strips.
class ScribbleController : public QObject {
    Q_OBJECT

  public:
    explicit ScribbleController(QObject* parent = nullptr);

    void setMixer(MixerProtocol* mixer);
    void setActorLibrary(ActorProfileLibrary* library);
    void setCueList(CueList* cueList);
    void setDCAMapping(DCAMapping* mapping) { m_dcaMapping = mapping; }

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled; }

    // active-channel highlight: color the strips of channels the current cue
    // touches (its actor-profile / level channels + its DCA-assigned channels)
    // and restore the rest, so the operator sees which faders a cue moves.
    void setHighlightEnabled(bool enabled);
    [[nodiscard]] bool isHighlightEnabled() const noexcept { return m_highlightEnabled; }
    void setHighlightColor(int color) { m_highlightColor = color; }
    [[nodiscard]] int highlightColor() const noexcept { return m_highlightColor; }

    // strip that shows the current cue number; 0 disables (no strip is used and
    // actor names are never suppressed). 1-based.
    void setCueNumberChannel(int channel);
    [[nodiscard]] int cueNumberChannel() const noexcept { return m_cueChannel; }

    // driver-mapped color index shown for each monitor state. Defaults suit the
    // X32 palette (white / blue / red); configurable so other consoles fit.
    void setStateColor(ChannelState state, int color);
    [[nodiscard]] int stateColor(ChannelState state) const;

    // push every assigned actor name to its channel now
    void refreshNames();

  public slots:
    void onChannelStateChanged(int channel, int state);
    void onCurrentCueChanged(int cueIndex);
    void onActorLibraryChanged();

  private:
    // channels the given cue touches (actor-profile + level + DCA-assigned)
    [[nodiscard]] QSet<int> channelsTouchedByCue(const Cue& cue) const;
    void updateHighlight(int cueIndex);

    MixerProtocol* m_mixer = nullptr;
    ActorProfileLibrary* m_library = nullptr;
    CueList* m_cueList = nullptr;
    DCAMapping* m_dcaMapping = nullptr;

    bool m_enabled = true;
    int m_cueChannel = 0;

    // indexed by int(ChannelState): Normal, Silent, Clipping
    int m_stateColors[3] = {7, 4, 1};

    bool m_highlightEnabled = false;
    int m_highlightColor = 2;             // driver palette index (X32: green)
    QSet<int> m_highlightedChannels;      // channels currently highlighted
    int m_currentCueIndex = -1;
};

} // namespace OpenMix
