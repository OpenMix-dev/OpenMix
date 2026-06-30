#include "ActorProfile.h"

#include <QJsonArray>

namespace OpenMix {

QJsonObject EqBand::toJson() const {
    QJsonObject json;
    json["band"] = band;
    json["on"] = on;
    json["type"] = type;
    json["freq"] = freq;
    json["gain"] = gain;
    json["q"] = q;
    return json;
}

EqBand EqBand::fromJson(const QJsonObject& json) {
    EqBand b;
    b.band = json["band"].toInt(1);
    b.on = json["on"].toBool(true);
    b.type = json["type"].toInt(0);
    b.freq = json["freq"].toDouble(1000.0);
    b.gain = json["gain"].toDouble(0.0);
    b.q = json["q"].toDouble(2.0);
    return b;
}

bool VoiceData::isEmpty() const {
    return !gainDb && !hpfOn && !hpfFreq && !eqOn && eqBands.isEmpty() && !dynOn &&
           !dynThreshold && !dynRatio && !dynAttack && !dynRelease && !dynGain;
}

QJsonObject VoiceData::toJson() const {
    QJsonObject json;
    if (gainDb)
        json["gain"] = *gainDb;
    if (hpfOn)
        json["hpfOn"] = *hpfOn;
    if (hpfFreq)
        json["hpfFreq"] = *hpfFreq;
    if (eqOn)
        json["eqOn"] = *eqOn;
    if (!eqBands.isEmpty()) {
        QJsonArray arr;
        for (const EqBand& b : eqBands)
            arr.append(b.toJson());
        json["eq"] = arr;
    }
    if (dynOn)
        json["dynOn"] = *dynOn;
    if (dynThreshold)
        json["dynThreshold"] = *dynThreshold;
    if (dynRatio)
        json["dynRatio"] = *dynRatio;
    if (dynAttack)
        json["dynAttack"] = *dynAttack;
    if (dynRelease)
        json["dynRelease"] = *dynRelease;
    if (dynGain)
        json["dynGain"] = *dynGain;
    return json;
}

VoiceData VoiceData::fromJson(const QJsonObject& json) {
    VoiceData v;
    if (json.contains("gain"))
        v.gainDb = json["gain"].toDouble();
    if (json.contains("hpfOn"))
        v.hpfOn = json["hpfOn"].toBool();
    if (json.contains("hpfFreq"))
        v.hpfFreq = json["hpfFreq"].toDouble();
    if (json.contains("eqOn"))
        v.eqOn = json["eqOn"].toBool();
    if (json.contains("eq")) {
        for (const QJsonValue& val : json["eq"].toArray())
            v.eqBands.append(EqBand::fromJson(val.toObject()));
    }
    if (json.contains("dynOn"))
        v.dynOn = json["dynOn"].toBool();
    if (json.contains("dynThreshold"))
        v.dynThreshold = json["dynThreshold"].toDouble();
    if (json.contains("dynRatio"))
        v.dynRatio = json["dynRatio"].toDouble();
    if (json.contains("dynAttack"))
        v.dynAttack = json["dynAttack"].toDouble();
    if (json.contains("dynRelease"))
        v.dynRelease = json["dynRelease"].toDouble();
    if (json.contains("dynGain"))
        v.dynGain = json["dynGain"].toDouble();
    return v;
}

QJsonObject ActorProfile::toJson() const {
    QJsonObject json;
    if (!m_main.isEmpty())
        json["main"] = m_main.toJson();
    if (!m_backup.isEmpty())
        json["backup"] = m_backup.toJson();
    return json;
}

ActorProfile ActorProfile::fromJson(const QJsonObject& json) {
    ActorProfile p;
    p.m_main = VoiceData::fromJson(json["main"].toObject());
    p.m_backup = VoiceData::fromJson(json["backup"].toObject());
    return p;
}

} // namespace OpenMix
