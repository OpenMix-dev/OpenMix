#pragma once

namespace OpenMix {

// Levels are carried in dB throughout OpenMix - in cues, across the protocol API,
// and into each driver, which encodes dB the way its console wants. A 0..1 fader
// position is not a level until a console-specific fader law is applied, so a
// position would make the same cue play back differently on different desks.
//
// dB below this read as -inf (fader fully down). JSON cannot carry an infinity
// and every console has its own -inf encoding, so the sentinel is a plain number
// below any console's floor. -999 and the +10 ceiling are the reference
// implementation's own values, which its Allen & Heath drivers clamp levels to.
inline constexpr double NEG_INF_DB = -999.0;

// the top of the throw on every console OpenMix speaks to
inline constexpr double MAX_DB = 10.0;

// the reference clamps levels to this floor before encoding, and treats anything
// under it as -inf
inline constexpr double MIN_DB = -95.0;

} // namespace OpenMix
