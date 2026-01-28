#pragma once

#include "CueList.h"
#include "DCAMapping.h"
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace OpenMix {

struct MixerConfig {
    QString type;     // protocol ID: "x32", "wing", "sq7", "cl5", etc.
    QString host;     // IP address or hostname
    int port = 10023; // default X32 port (will be updated per console type)
    int dcaCount = 8; // number of DCAs (varies by console)

    QJsonObject toJson() const;
    static MixerConfig fromJson(const QJsonObject& json);
};

class Show : public QObject {
    Q_OBJECT

  public:
    explicit Show(QObject* parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString& name);

    QString author() const { return m_author; }
    void setAuthor(const QString& author) { m_author = author; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; }

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path) { m_filePath = path; }

    bool isModified() const;
    void setModified(bool modified);
    void checkModifiedState();

    CueList* cueList() { return &m_cueList; }
    const CueList* cueList() const { return &m_cueList; }

    DCAMapping* dcaMapping() { return &m_dcaMapping; }
    const DCAMapping* dcaMapping() const { return &m_dcaMapping; }

    MixerConfig mixerConfig() const { return m_mixerConfig; }
    void setMixerConfig(const MixerConfig& config) { m_mixerConfig = config; }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    void newShow();

  signals:
    void nameChanged(const QString& name);
    void modifiedChanged(bool modified);

  private:
    void connectCueListSignals();
    void connectDcaMappingSignals();

    QString m_name;
    QString m_author;
    QString m_notes;
    QString m_filePath;
    CueList m_cueList;
    MixerConfig m_mixerConfig;
    DCAMapping m_dcaMapping;
    QJsonObject m_originalState;
    bool m_lastEmittedModified = false;
};

} // namespace OpenMix
