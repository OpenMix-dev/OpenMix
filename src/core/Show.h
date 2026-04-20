#pragma once

#include "CueList.h"
#include "DCAMapping.h"
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace OpenMix {

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

    [[nodiscard]] MixerConfig mixerConfig() const { return m_mixerConfig; }
    void setMixerConfig(const MixerConfig& config) { m_mixerConfig = config; checkModifiedState(); }

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
    bool m_isDirty = false;
};

} // namespace OpenMix
