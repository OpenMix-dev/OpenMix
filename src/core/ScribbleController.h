#pragma once

#include "ChannelMonitor.h" // ChannelState
#include <QObject>
#include <QString>

namespace OpenMix {

class MixerProtocol;
class ActorProfileLibrary;
class CueList;

// Drives console scribble strips from show data. It writes each channel's
// assigned actor name to that channel's strip, writes the current cue number to
// a configurable strip, and colours strips from ChannelMonitor silence/clip
// state. All pushes go through MixerProtocol::setChannelName / setChannelColour,
// which are no-ops on drivers that cannot address scribble strips.
class ScribbleController : public QObject {
    Q_OBJECT

  public:
    explicit ScribbleController(QObject* parent = nullptr);

    void setMixer(MixerProtocol* mixer);
    void setActorLibrary(ActorProfileLibrary* library);
    void setCueList(CueList* cueList);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept { return m_enabled; }

    // strip that shows the current cue number; 0 disables (no strip is used and
    // actor names are never suppressed). 1-based.
    void setCueNumberChannel(int channel);
    [[nodiscard]] int cueNumberChannel() const noexcept { return m_cueChannel; }

    // driver-mapped colour index shown for each monitor state. Defaults suit the
    // X32 palette (white / blue / red); configurable so other consoles fit.
    void setStateColour(ChannelState state, int colour);
    [[nodiscard]] int stateColour(ChannelState state) const;

    // push every assigned actor name to its channel now
    void refreshNames();

  public slots:
    void onChannelStateChanged(int channel, int state);
    void onCurrentCueChanged(int cueIndex);
    void onActorLibraryChanged();

  private:
    MixerProtocol* m_mixer = nullptr;
    ActorProfileLibrary* m_library = nullptr;
    CueList* m_cueList = nullptr;

    bool m_enabled = true;
    int m_cueChannel = 0;

    // indexed by int(ChannelState): Normal, Silent, Clipping
    int m_stateColours[3] = {7, 4, 1};
};

} // namespace OpenMix
