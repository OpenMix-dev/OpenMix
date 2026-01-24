#pragma once

#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

namespace OpenMix {

struct PlaybackLogEntry {
    QDateTime timestamp;

    enum Type {
        CueExecuted,
        MacroExpanded,
        AutoFollowTriggered,
        ValidationWarning,
        ValidationError,
        EmergencyStop,
        GoLockout,
        ParameterSent,
        StateCapture,
        Custom
    };
    Type type;

    QString cueId;
    QString details;
    QJsonObject parameters;

    // convert type to human-readable string
    QString typeString() const;

    // serialize to JSON for file export
    QJsonObject toJson() const;
    static PlaybackLogEntry fromJson(const QJsonObject& json);
};

class PlaybackLogger : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackLogger(QObject* parent = nullptr);
    ~PlaybackLogger();

    // file logging
    void setLogFile(const QString& path);
    QString logFilePath() const { return m_logFilePath; }
    bool isFileLoggingEnabled() const { return m_logFile.isOpen(); }
    void closeLogFile();

    // maximum entries to keep in memory (0 = unlimited)
    void setMaxMemoryEntries(int max) { m_maxMemoryEntries = max; }
    int maxMemoryEntries() const { return m_maxMemoryEntries; }

    // log entries
    void log(PlaybackLogEntry::Type type, const QString& cueId, const QString& details,
             const QJsonObject& params = {});

    void logCueExecuted(const QString& cueId, const QString& cueName, double cueNumber);
    void logMacroExpanded(const QString& parentId, const QString& childId);
    void logAutoFollowTriggered(const QString& cueId, double delay);
    void logValidationWarning(const QString& cueId, const QString& message);
    void logValidationError(const QString& cueId, const QString& message);
    void logEmergencyStop(const QString& reason);
    void logGoLockout(const QString& reason);
    void logParameterSent(const QString& path, const QVariant& value);
    void logStateCapture(const QString& description, const QJsonObject& state);

    // retrieve logged entries
    QVector<PlaybackLogEntry> allEntries() const;
    QVector<PlaybackLogEntry> recentEntries(int count = 100) const;
    QVector<PlaybackLogEntry> entriesSince(const QDateTime& since) const;
    QVector<PlaybackLogEntry> entriesForCue(const QString& cueId) const;

    // export
    bool exportToFile(const QString& path) const;
    bool exportToCSV(const QString& path) const;

    // clear logs
    void clear();

  signals:
    void entryAdded(const PlaybackLogEntry& entry);
    void logCleared();

  private:
    void addEntry(const PlaybackLogEntry& entry);
    void writeToFile(const PlaybackLogEntry& entry);
    void pruneOldEntries();

    mutable QMutex m_mutex;
    QVector<PlaybackLogEntry> m_entries;
    int m_maxMemoryEntries = 10000; // default 10k entries in memory

    QString m_logFilePath;
    QFile m_logFile;
};

} // namespace OpenMix
