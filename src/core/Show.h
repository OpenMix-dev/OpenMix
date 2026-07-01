#pragma once

#include "ActorProfileLibrary.h"
#include "CueList.h"
#include "CueZero.h"
#include "DCAMapping.h"
#include "Ensemble.h"
#include "Position.h"
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

namespace OpenMix {

class MixerProtocol;

struct MixerConfig {
    static constexpr int DEFAULT_PORT = 10023;
    static constexpr int DEFAULT_DCA_COUNT = 8;

    QString type;     // protocol ID: "x32", "wing", "sq7", "cl5", etc.
    QString host;     // IP address or hostname
    int port = DEFAULT_PORT;
    int dcaCount = DEFAULT_DCA_COUNT;

    QJsonObject toJson() const;
    [[nodiscard]] static MixerConfig fromJson(const QJsonObject& json);
};

class Show : public QObject {
    Q_OBJECT

  public:
    explicit Show(QObject* parent = nullptr);

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name);

    [[nodiscard]] QString author() const { return m_author; }
    void setAuthor(const QString& author) { m_author = author; checkModifiedState(); }

    [[nodiscard]] QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; checkModifiedState(); }

    [[nodiscard]] QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path) { m_filePath = path; }

    [[nodiscard]] bool isModified() const;
    void setModified(bool modified);
    void checkModifiedState();

    [[nodiscard]] CueList* cueList() { return &m_cueList; }
    [[nodiscard]] const CueList* cueList() const { return &m_cueList; }

    [[nodiscard]] DCAMapping* dcaMapping() { return &m_dcaMapping; }
    [[nodiscard]] const DCAMapping* dcaMapping() const { return &m_dcaMapping; }

    [[nodiscard]] ActorProfileLibrary* actorProfileLibrary() { return &m_actorProfileLibrary; }
    [[nodiscard]] const ActorProfileLibrary* actorProfileLibrary() const {
        return &m_actorProfileLibrary;
    }

    [[nodiscard]] PositionLibrary* positionLibrary() { return &m_positionLibrary; }
    [[nodiscard]] const PositionLibrary* positionLibrary() const { return &m_positionLibrary; }

    [[nodiscard]] EnsembleLibrary* ensembleLibrary() { return &m_ensembleLibrary; }
    [[nodiscard]] const EnsembleLibrary* ensembleLibrary() const { return &m_ensembleLibrary; }

    [[nodiscard]] CueZero* cueZero() { return &m_cueZero; }
    [[nodiscard]] const CueZero* cueZero() const { return &m_cueZero; }

    // recall the show's Cue Zero base state onto a connected mixer
    void applyCueZero(MixerProtocol* mixer) const { m_cueZero.apply(mixer); }

    [[nodiscard]] MixerConfig mixerConfig() const { return m_mixerConfig; }
    void setMixerConfig(const MixerConfig& config) { m_mixerConfig = config; checkModifiedState(); }

    // ganged input-channel pairs; on fire a level applied to one channel is
    // mirrored to its partner. Show-level, shared across all cues.
    [[nodiscard]] QList<QPair<int, int>> channelGangs() const { return m_channelGangs; }
    void setChannelGangs(const QList<QPair<int, int>>& gangs) {
        m_channelGangs = gangs;
        checkModifiedState();
    }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    void newShow();

  signals:
    void nameChanged(const QString& name);
    void modifiedChanged(bool modified);

  private:
    void connectCueListSignals();
    void connectDcaMappingSignals();
    void connectActorLibrarySignals();
    void connectPositionLibrarySignals();
    void connectEnsembleLibrarySignals();
    void connectCueZeroSignals();

    QString m_name;
    QString m_author;
    QString m_notes;
    QString m_filePath;
    CueList m_cueList;
    MixerConfig m_mixerConfig;
    QList<QPair<int, int>> m_channelGangs; // ganged input-channel pairs
    DCAMapping m_dcaMapping;
    ActorProfileLibrary m_actorProfileLibrary;
    PositionLibrary m_positionLibrary;
    EnsembleLibrary m_ensembleLibrary;
    CueZero m_cueZero;
    bool m_isDirty = false;
};

} // namespace OpenMix
