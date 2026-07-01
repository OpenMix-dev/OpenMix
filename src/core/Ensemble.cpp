#include "Ensemble.h"

#include <QJsonArray>
#include <QUuid>
#include <algorithm>

namespace OpenMix {

namespace {
// sort ascending, drop duplicates and non-positive channels
QList<int> normalizeChannels(QList<int> channels) {
    std::sort(channels.begin(), channels.end());
    channels.erase(std::unique(channels.begin(), channels.end()), channels.end());
    channels.erase(std::remove_if(channels.begin(), channels.end(), [](int c) { return c <= 0; }),
                   channels.end());
    return channels;
}
} // namespace

Ensemble::Ensemble() : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

Ensemble::Ensemble(const QString& name)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_name(name) {}

void Ensemble::regenerateId() { m_id = QUuid::createUuid().toString(QUuid::WithoutBraces); }

void Ensemble::setChannels(const QList<int>& channels) { m_channels = normalizeChannels(channels); }

void Ensemble::addChannel(int channel) {
    if (channel <= 0 || m_channels.contains(channel))
        return;
    m_channels.append(channel);
    std::sort(m_channels.begin(), m_channels.end());
}

void Ensemble::removeChannel(int channel) { m_channels.removeAll(channel); }

void Ensemble::setProfileSlot(const QString& slot) {
    m_profileSlot = slot.isEmpty() ? QString(DEFAULT_SLOT) : slot;
}

QJsonObject Ensemble::toJson() const {
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["profileSlot"] = m_profileSlot;

    QJsonArray channelsArr;
    for (int ch : m_channels)
        channelsArr.append(ch);
    json["channels"] = channelsArr;

    return json;
}

Ensemble Ensemble::fromJson(const QJsonObject& json) {
    Ensemble ensemble;
    ensemble.m_id = json["id"].toString();
    if (ensemble.m_id.isEmpty())
        ensemble.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ensemble.m_name = json["name"].toString();
    ensemble.m_profileSlot = json["profileSlot"].toString(DEFAULT_SLOT);
    if (ensemble.m_profileSlot.isEmpty())
        ensemble.m_profileSlot = DEFAULT_SLOT;

    QList<int> channels;
    for (const QJsonValue& val : json["channels"].toArray())
        channels.append(val.toInt());
    ensemble.m_channels = normalizeChannels(channels);

    return ensemble;
}

EnsembleLibrary::EnsembleLibrary(QObject* parent) : QObject(parent) {}

int EnsembleLibrary::indexOfEnsemble(const QString& id) const {
    for (int i = 0; i < m_ensembles.size(); ++i) {
        if (m_ensembles[i].id() == id)
            return i;
    }
    return -1;
}

const Ensemble* EnsembleLibrary::ensembleById(const QString& id) const {
    int idx = indexOfEnsemble(id);
    return idx >= 0 ? &m_ensembles[idx] : nullptr;
}

QList<const Ensemble*> EnsembleLibrary::ensemblesForChannel(int channel) const {
    QList<const Ensemble*> result;
    for (const Ensemble& e : m_ensembles) {
        if (e.hasChannel(channel))
            result.append(&e);
    }
    return result;
}

void EnsembleLibrary::addEnsemble(const Ensemble& ensemble) {
    m_ensembles.append(ensemble);
    emit ensembleAdded(ensemble.id());
    emit changed();
}

void EnsembleLibrary::updateEnsemble(const QString& id, const Ensemble& ensemble) {
    int idx = indexOfEnsemble(id);
    if (idx < 0)
        return;
    m_ensembles[idx] = ensemble;
    emit ensembleModified(id);
    emit changed();
}

void EnsembleLibrary::removeEnsemble(const QString& id) {
    int idx = indexOfEnsemble(id);
    if (idx < 0)
        return;
    m_ensembles.removeAt(idx);
    emit ensembleRemoved(id);
    emit changed();
}

void EnsembleLibrary::clear() {
    m_ensembles.clear();
    emit changed();
}

QJsonObject EnsembleLibrary::toJson() const {
    QJsonObject json;

    QJsonArray ensemblesArr;
    for (const Ensemble& e : m_ensembles)
        ensemblesArr.append(e.toJson());
    json["ensembles"] = ensemblesArr;

    return json;
}

void EnsembleLibrary::loadFromJson(const QJsonObject& json) {
    m_ensembles.clear();

    for (const QJsonValue& val : json["ensembles"].toArray())
        m_ensembles.append(Ensemble::fromJson(val.toObject()));

    emit changed();
}

} // namespace OpenMix
