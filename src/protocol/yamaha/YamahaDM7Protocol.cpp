#include "YamahaDM7Protocol.h"
#include <algorithm>
#include <cmath>

namespace OpenMix {

YamahaDM7Protocol::YamahaDM7Protocol(const MixerCapabilities& caps, QObject* parent)
    : YamahaProtocol(caps, parent) {}

int YamahaDM7Protocol::preampGainToScp(double gainDb) const {
    return std::clamp(static_cast<int>(std::lround(gainDb)), -6, 66);
}

} // namespace OpenMix
