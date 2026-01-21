#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace OpenMix {

class CueList;
class MixerProtocol;
class PreviewLayer;

enum class LiveEditMode { Inactive, Live, Preview };

struct ParameterEdit {
    QString path;
    QVariant originalValue;
    QVariant currentValue;
    qint64 timestamp;
};

class LiveEditSession : public QObject {
    Q_OBJECT

  public:
    explicit LiveEditSession(QObject* parent = nullptr);

    // dependencies
    void setCueList(CueList* cueList);
    void setMixer(MixerProtocol* mixer);
    void setPreviewLayer(PreviewLayer* previewLayer);

    // state queries
    LiveEditMode mode() const { return m_mode; }
    bool isActive() const { return m_mode != LiveEditMode::Inactive; }
    bool isPreview() const { return m_mode == LiveEditMode::Preview; }
    bool isLive() const { return m_mode == LiveEditMode::Live; }

    QString activeCueId() const { return m_activeCueId; }
    int activeCueIndex() const;

    // pending edits
    QJsonObject pendingEdits() const;
    QStringList editedPaths() const;
    bool hasEdits() const;
    int editCount() const;

    // original values (from cue or mixer at session start)
    QJsonObject originalValues() const { return m_originalValues; }
    QVariant originalValue(const QString& path) const;

    // get a specific edit
    const ParameterEdit* edit(const QString& path) const;

  public slots:
    // session control
    void startLiveEdit(const QString& cueId);
    void startLiveEditByIndex(int cueIndex);
    void startPreview(const QString& cueId);
    void startPreviewByIndex(int cueIndex);
    void togglePreviewMode();
    void cancel();

    // parameter editing
    void setParameter(const QString& path, const QVariant& value);
    void revertParameter(const QString& path);
    void revertAll();

    // commit changes
    void commitToCue(const QString& targetCueId);
    void commitToCurrentCue();
    void commitToNewCue(const QString& cueName = QString());

  signals:
    void modeChanged(LiveEditMode mode);
    void sessionStarted(const QString& cueId);
    void sessionEnded();
    void parameterEdited(const QString& path, const QVariant& oldValue, const QVariant& newValue);
    void parameterReverted(const QString& path);
    void editsCommitted(const QString& cueId);
    void editsCancelled();

  private:
    void setMode(LiveEditMode mode);
    void startSession(const QString& cueId, LiveEditMode mode);
    void captureOriginalState();
    void applyEditToMixer(const QString& path, const QVariant& value);
    void restoreOriginalValues();

    LiveEditMode m_mode = LiveEditMode::Inactive;
    QString m_activeCueId;
    QJsonObject m_originalValues;
    QMap<QString, ParameterEdit> m_edits;

    CueList* m_cueList = nullptr;
    MixerProtocol* m_mixer = nullptr;
    PreviewLayer* m_previewLayer = nullptr;
};

} // namespace OpenMix
