#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>

namespace OpenMix {

class DCAMapping : public QObject {
    Q_OBJECT

  public:
    explicit DCAMapping(QObject* parent = nullptr);

    // channel assignments
    void assignChannelToDCA(int channel, int dca);
    void removeChannelFromDCA(int channel, int dca);
    void clearChannelFromAllDCAs(int channel);
    QList<int> channelsForDCA(int dca) const;
    int dcaForChannel(int channel) const; // -1 = unassigned
    bool isChannelAssigned(int channel) const;

    // bus assignments
    void assignBusToDCA(int bus, int dca);
    void removeBusFromDCA(int bus, int dca);
    void clearBusFromAllDCAs(int bus);
    QList<int> busesForDCA(int dca) const;
    int dcaForBus(int bus) const; // -1 = unassigned
    bool isBusAssigned(int bus) const;

    // bus names
    void setBusName(int bus, const QString& name);
    QString busName(int bus) const;
    QMap<int, QString> busNames() const { return m_busNames; }

    // bulk operations
    void clear();
    void setChannelAssignments(const QMap<int, QList<int>>& assignments);
    void setBusAssignments(const QMap<int, QList<int>>& assignments);
    QMap<int, QList<int>> channelAssignments() const { return m_channelAssignments; }
    QMap<int, QList<int>> busAssignments() const { return m_busAssignments; }

    QSet<int> assignedDCAs() const;

    // serialization
    QJsonObject toJson() const;
    static DCAMapping* fromJson(const QJsonObject& json, QObject* parent = nullptr);
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
};

} // namespace OpenMix
