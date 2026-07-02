#include "ChannelUtilization.h"
#include "Cue.h"
#include "CueList.h"
#include "DCAMapping.h"
#include "Show.h"

#include <QMap>
#include <QSet>
#include <algorithm>

namespace OpenMix {

QList<ChannelUsage> computeChannelUtilization(const Show* show) {
    QList<ChannelUsage> result;
    if (!show)
        return result;

    const CueList* cues = show->cueList();
    const QMap<int, QList<int>> showMapping = show->dcaMapping()->channelAssignments();

    // channel -> ordered, de-duplicated cue numbers
    QMap<int, QList<double>> byChannel;

    auto note = [&byChannel](int channel, double cueNumber) {
        QList<double>& list = byChannel[channel];
        if (list.isEmpty() || list.last() != cueNumber)
            list.append(cueNumber);
    };

    for (int i = 0; i < cues->count(); ++i) {
        const Cue& cue = cues->at(i);
        const double number = cue.number();

        QSet<int> channels;

        const QMap<int, QList<int>>& mapping =
            cue.hasCustomDCAMapping() ? cue.dcaChannelMapping() : showMapping;
        for (auto it = mapping.begin(); it != mapping.end(); ++it)
            for (int ch : it.value())
                channels.insert(ch);

        for (int ch : cue.channelProfiles().keys())
            channels.insert(ch);
        for (int ch : cue.channelLevels().keys())
            channels.insert(ch);
        for (int ch : cue.channelPositions().keys())
            channels.insert(ch);

        for (int ch : channels)
            note(ch, number);
    }

    result.reserve(byChannel.size());
    for (auto it = byChannel.begin(); it != byChannel.end(); ++it) {
        std::sort(it.value().begin(), it.value().end());
        result.append({it.key(), it.value()});
    }
    return result;
}

} // namespace OpenMix
