#pragma once

namespace OpenMix {

// Levels are dB everywhere: in cues, across the protocol API, and into each
// driver, which encodes dB the way its console wants. A 0..1 position is not a
// level until a console applies its fader law, so the same cue would play back
// differently per desk.

// dB at or below this is -inf. JSON cannot carry an infinity and each console
// encodes -inf its own way, so the sentinel is a plain number below every floor.
inline constexpr double NEG_INF_DB = -999.0;

// the top of the throw on every console OpenMix speaks to
inline constexpr double MAX_DB = 10.0;

// the floor levels are clamped to before encoding; under it is -inf
inline constexpr double MIN_DB = -95.0;

} // namespace OpenMix
