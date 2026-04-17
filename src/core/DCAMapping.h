#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <optional>

namespace OpenMix {

class DCAMapping : public QObject {
    Q_OBJECT

  public:
    explicit DCAMapping(QObject* parent = nullptr);

    // channel assignments
    void assignChannelToDCA(int channel, int dca);
    void removeChannelFromDCA(int channel, int dca);
    void clearChannelFromAllDCAs(int channel);
    [[nodiscard]] QList<int> channelsForDCA(int dca) const;
    [[nodiscard]] std::optional<int> dcaForChannel(int channel) const;
    [[nodiscard]] bool isChannelAssigned(int channel) const;

    // bus assignments
    void assignBusToDCA(int bus, int dca);
    void removeBusFromDCA(int bus, int dca);
    void clearBusFromAllDCAs(int bus);
    [[nodiscard]] QList<int> busesForDCA(int dca) const;
    [[nodiscard]] std::optional<int> dcaForBus(int bus) const;
    [[nodiscard]] bool isBusAssigned(int bus) const;

    // bus names
    void setBusName(int bus, const QString& name);
    [[nodiscard]] QString busName(int bus) const;
    [[nodiscard]] QMap<int, QString> busNames() const { return m_busNames; }

    // bulk operations
    void clear();
    void setChannelAssignments(const QMap<int, QList<int>>& assignments);
    void setBusAssignments(const QMap<int, QList<int>>& assignments);
    [[nodiscard]] QMap<int, QList<int>> channelAssignments() const { return m_channelAssignments; }
    [[nodiscard]] QMap<int, QList<int>> busAssignments() const { return m_busAssignments; }

    [[nodiscard]] QSet<int> assignedDCAs() const;

    // serialization
    QJsonObject toJson() const;
    [[nodiscard]] static DCAMapping* fromJson(const QJsonObject& json, QObject* parent = nullptr);
    void loadFromJson(const QJsonObject& json);

  signals:
    void channelAssignmentChanged(int channel, int dca);
    void busAssignmentChanged(int bus, int dca);
    void busNameChanged(int bus, const QString& name);
    void mappingCleared();

  private:
    // DCA# -> [channel#, ...]
    QMap<int, QList<int>> m_channelAssignments;
    // DCA# -> [bus#, ...]
    QMap<int, QList<int>> m_busAssignments;
    // bus# -> user-defined name
    QMap<int, QString> m_busNames;

    static void clearFromAllDCAs(QMap<int, QList<int>>& map, int id);
    static QJsonObject mappingToJsonObject(const QMap<int, QList<int>>& map);
    static QMap<int, QList<int>> jsonObjectToMapping(const QJsonObject& obj);
};

} // namespace OpenMix
