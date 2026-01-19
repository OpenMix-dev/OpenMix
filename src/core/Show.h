#pragma once

#include "CueList.h"
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace StageBlend {

struct MixerConfig {
    QString type; // "x32", "yamaha", "allen-heath"
    QString host;
    int port = 10023; // default X32 port

    QJsonObject toJson() const;
    static MixerConfig fromJson(const QJsonObject& json);
};

class Show : public QObject {
    Q_OBJECT

  public:
    explicit Show(QObject* parent = nullptr);

    // project metadata
    QString name() const { return m_name; }
    void setName(const QString& name);

    QString author() const { return m_author; }
    void setAuthor(const QString& author) { m_author = author; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& notes) { m_notes = notes; }

    // file path
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path) { m_filePath = path; }

    // modified state
    bool isModified() const { return m_modified; }
    void setModified(bool modified);

    // cue list
    CueList* cueList() { return &m_cueList; }
    const CueList* cueList() const { return &m_cueList; }

    // mixer configuration
    MixerConfig mixerConfig() const { return m_mixerConfig; }
    void setMixerConfig(const MixerConfig& config) { m_mixerConfig = config; }

    // serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    // create new empty show
    void newShow();

  signals:
    void nameChanged(const QString& name);
    void modifiedChanged(bool modified);

  private:
    void connectCueListSignals();

    QString m_name;
    QString m_author;
    QString m_notes;
    QString m_filePath;
    bool m_modified = false;
    CueList m_cueList;
    MixerConfig m_mixerConfig;
};

} // namespace StageBlend
