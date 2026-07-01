#pragma once

#include <QList>

namespace OpenMix {

class Show;

// One input channel and the cue numbers that reference it.
struct ChannelUsage {
    int channel = 0;
    QList<double> cueNumbers; // cues that touch this channel, in list order
};

// Compute, for every input channel referenced anywhere in the show, the list of
// cues that use it. A cue uses a channel when the channel appears in the cue's
// effective DCA mapping (its own custom mapping, else the show mapping) or in the
// cue's per-channel profile / level / position assignments. Sorted by channel.
[[nodiscard]] QList<ChannelUsage> computeChannelUtilization(const Show* show);

} // namespace OpenMix
