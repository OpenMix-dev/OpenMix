#pragma once

#include <QString>

namespace OpenMix {

enum class FadeCurve { Linear, EaseInOut, EaseIn, EaseOut };

inline QString fadeCurveToString(FadeCurve curve) {
    switch (curve) {
    case FadeCurve::Linear:
        return "linear";
    case FadeCurve::EaseInOut:
        return "easeInOut";
    case FadeCurve::EaseIn:
        return "easeIn";
    case FadeCurve::EaseOut:
        return "easeOut";
    }
    return "linear";
}

inline FadeCurve stringToFadeCurve(const QString& str) {
    if (str == "easeInOut")
        return FadeCurve::EaseInOut;
    if (str == "easeIn")
        return FadeCurve::EaseIn;
    if (str == "easeOut")
        return FadeCurve::EaseOut;
    return FadeCurve::Linear;
}

} // namespace OpenMix
