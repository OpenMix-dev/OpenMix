#pragma once

#include "ActorProfileLibrary.h"
#include "ConsoleNameCache.h"
#include "CueList.h"
#include "CueZero.h"
#include "DCAMapping.h"
#include "Ensemble.h"
#include "FxLibrary.h"
#include "Position.h"
#include "SpareBackup.h"
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>

namespace OpenMix {

class MixerProtocol;

struct MixerConfig {
    static constexpr int DEFAULT_PORT = 10023;
    static constexpr int DEFAULT_DCA_COUNT = 8;

    QString type; // protocol ID: "x32", "wing", "sq7", "cl5", etc.
    QString host; // IP address or hostname
    int port = DEFAULT_PORT;
    int dcaCount = DEFAULT_DCA_COUNT;

    // Which curve an SQ maps NRPN levels through. It is a console-side setting
    // (Utility > General > MIDI > NRPN Fader Law) that cannot be read back, so
    // the desk and the show have to agree: "linear" (the console's standard
    // mode) or "audio". Ignored by consoles without the setting.
    QString faderLaw = "linear";

    // GLD stamps its MIDI channel into every message and cannot report it, so it
    // has to match Setup / Control on the console. 1-16.
    int midiChannel = 1;

    // DiGiCo's OSC addresses are the operator's to supply: the syntax varies by
    // model and software version. "*" stands for the channel; an empty pattern
    // disables that operation (see DiGiCoProtocol).
    QString oscChannelFader; // e.g. "/ch/*/fader"
    QString oscChannelMute;  // e.g. "/ch/*/mute"
    QString oscSceneRecall;  // e.g. "/snapshot/fire"
    int oscReceivePort = 8000;

    [[nodiscard]] bool operator==(const MixerConfig& other) const;
    [[nodiscard]] bool operator!=(const MixerConfig& other) const { return !(*this == other); }

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
    void setAuthor(const QString& author) {
        m_author = author;
        checkModifiedState();
    }

    [[nodiscard]] QString designer() const { return m_designer; }
    void setDesigner(const QString& designer) {
        m_designer = designer;
        checkModifiedState();
    }

    [[nodiscard]] QString notes() const { return m_notes; }
    void setNotes(const QString& notes) {
        m_notes = notes;
        checkModifiedState();
    }

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

    [[nodiscard]] SpareBackup* spareBackup() { return &m_spareBackup; }
    [[nodiscard]] const SpareBackup* spareBackup() const { return &m_spareBackup; }

    [[nodiscard]] FxLibrary* fxLibrary() { return &m_fxLibrary; }
    [[nodiscard]] const FxLibrary* fxLibrary() const { return &m_fxLibrary; }

    [[nodiscard]] ConsoleNameCache* consoleNameCache() { return &m_consoleNameCache; }
    [[nodiscard]] const ConsoleNameCache* consoleNameCache() const { return &m_consoleNameCache; }

    // recall the show's Cue Zero base state onto a connected mixer
    void applyCueZero(MixerProtocol* mixer) const { m_cueZero.apply(mixer); }

    [[nodiscard]] MixerConfig mixerConfig() const { return m_mixerConfig; }
    void setMixerConfig(const MixerConfig& config);

    // ganged input-channel pairs; on fire a level applied to one channel is
    // mirrored to its partner. Show-level, shared across all cues.
    [[nodiscard]] QList<QPair<int, int>> channelGangs() const { return m_channelGangs; }
    void setChannelGangs(const QList<QPair<int, int>>& gangs);

    // optional per-gang label/color, aligned by gang index; cosmetic only.
    void setGangName(int index, const QString& name);
    [[nodiscard]] QString gangName(int index) const;
    void setGangColor(int index, const QString& color);
    [[nodiscard]] QString gangColor(int index) const;

    // console-behavior preferences. muteDcaUnassign and dimDcaFaders drive real
    // DCA-apply behavior via PlaybackEngine; selectOnSpill and suppressBackupSwitch
    // are stored preferences the app reads.
    [[nodiscard]] bool dimDcaFaders() const noexcept { return m_dimDcaFaders; }
    void setDimDcaFaders(bool on) {
        m_dimDcaFaders = on;
        checkModifiedState();
    }
    [[nodiscard]] bool selectOnSpill() const noexcept { return m_selectOnSpill; }
    void setSelectOnSpill(bool on) {
        m_selectOnSpill = on;
        checkModifiedState();
    }
    [[nodiscard]] bool muteDcaUnassign() const noexcept { return m_muteDcaUnassign; }
    void setMuteDcaUnassign(bool on) {
        m_muteDcaUnassign = on;
        checkModifiedState();
    }
    [[nodiscard]] bool suppressBackupSwitch() const noexcept { return m_suppressBackupSwitch; }
    void setSuppressBackupSwitch(bool on) {
        m_suppressBackupSwitch = on;
        checkModifiedState();
    }

    // DCAs OpenMix does not control: hidden from the cue list, greyed in the
    // mapping panel, and never written to on the console during playback.
    // Stored inverse so old shows (no key) and newly-available DCAs default
    // to active.
    [[nodiscard]] QSet<int> inactiveDcas() const { return m_inactiveDcas; }
    [[nodiscard]] bool isDcaActive(int dca) const { return !m_inactiveDcas.contains(dca); }
    void setDcaActive(int dca, bool active);
    void setInactiveDcas(const QSet<int>& dcas);

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    void newShow();

  signals:
    void nameChanged(const QString& name);
    void modifiedChanged(bool modified);
    void mixerConfigChanged();
    void activeDcasChanged();

  private:
    void connectCueListSignals();
    void connectDcaMappingSignals();
    void connectActorLibrarySignals();
    void connectPositionLibrarySignals();
    void connectEnsembleLibrarySignals();
    void connectCueZeroSignals();

    QString m_name;
    QString m_author;
    QString m_designer;
    QString m_notes;
    QString m_filePath;
    CueList m_cueList;
    MixerConfig m_mixerConfig;
    QList<QPair<int, int>> m_channelGangs;            // ganged input-channel pairs
    QList<QPair<QString, QString>> m_channelGangMeta; // per-gang (name, color), by index

    bool m_dimDcaFaders = false;
    bool m_selectOnSpill = false;
    bool m_muteDcaUnassign = false;
    bool m_suppressBackupSwitch = false;
    QSet<int> m_inactiveDcas;

    DCAMapping m_dcaMapping;
    ActorProfileLibrary m_actorProfileLibrary;
    PositionLibrary m_positionLibrary;
    EnsembleLibrary m_ensembleLibrary;
    CueZero m_cueZero;
    SpareBackup m_spareBackup;
    FxLibrary m_fxLibrary;
    ConsoleNameCache m_consoleNameCache;
    bool m_isDirty = false;
};

} // namespace OpenMix
