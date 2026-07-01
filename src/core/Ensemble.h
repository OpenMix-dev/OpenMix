#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace OpenMix {

// A named group of input channels that share one actor-profile slot (e.g. a
// chorus of hand-mics all recalled on their "Main" voice). Channels are stored
// 1-based and kept sorted/unique.
class Ensemble {
  public:
    static constexpr const char* DEFAULT_SLOT = "Main";

    Ensemble();
    explicit Ensemble(const QString& name);

    [[nodiscard]] QString id() const { return m_id; }
    void setId(const QString& id) { m_id = id; }
    void regenerateId();

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    [[nodiscard]] const QList<int>& channels() const { return m_channels; }
    void setChannels(const QList<int>& channels);
    void addChannel(int channel);
    void removeChannel(int channel);
    [[nodiscard]] bool hasChannel(int channel) const { return m_channels.contains(channel); }
    void clearChannels() { m_channels.clear(); }

    // the profile slot each member channel resolves against, default "Main"
    [[nodiscard]] QString profileSlot() const { return m_profileSlot; }
    void setProfileSlot(const QString& slot);

    QJsonObject toJson() const;
    [[nodiscard]] static Ensemble fromJson(const QJsonObject& json);

  private:
    QString m_id;
    QString m_name;
    QList<int> m_channels;
    QString m_profileSlot{DEFAULT_SLOT};
};

// Owns the show's ensembles. Mirrors ActorProfileLibrary: add/update/remove by
// id, JSON round-trip, and change signals so the Show can track its dirty state.
// Lives on the Show.
class EnsembleLibrary : public QObject {
    Q_OBJECT

  public:
    explicit EnsembleLibrary(QObject* parent = nullptr);

    [[nodiscard]] const QList<Ensemble>& ensembles() const { return m_ensembles; }
    [[nodiscard]] int count() const { return m_ensembles.size(); }
    [[nodiscard]] const Ensemble* ensembleById(const QString& id) const;

    // ensembles that contain the given channel (a channel may join several)
    [[nodiscard]] QList<const Ensemble*> ensemblesForChannel(int channel) const;

    void addEnsemble(const Ensemble& ensemble);
    void updateEnsemble(const QString& id, const Ensemble& ensemble);
    void removeEnsemble(const QString& id);
    void clear();

    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject& json);

  signals:
    void changed();
    void ensembleAdded(const QString& id);
    void ensembleModified(const QString& id);
    void ensembleRemoved(const QString& id);

  private:
    [[nodiscard]] int indexOfEnsemble(const QString& id) const;

    QList<Ensemble> m_ensembles;
};

} // namespace OpenMix
