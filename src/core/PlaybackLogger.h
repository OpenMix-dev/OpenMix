#pragma once

#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

namespace OpenMix {

struct PlaybackLogEntry {
    QDateTime timestamp;

    enum class Type {
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
    [[nodiscard]] static PlaybackLogEntry fromJson(const QJsonObject& json);
};

class PlaybackLogger : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackLogger(QObject* parent = nullptr);
    ~PlaybackLogger();

    // file logging
    void setLogFile(const QString& path);
    [[nodiscard]] QString logFilePath() const { return m_logFilePath; }
    [[nodiscard]] bool isFileLoggingEnabled() const { return m_logFile.isOpen(); }
    void closeLogFile();

    // maximum entries to keep in memory (0 = unlimited)
    void setMaxMemoryEntries(int max) { m_maxMemoryEntries = max; }
    [[nodiscard]] int maxMemoryEntries() const noexcept { return m_maxMemoryEntries; }

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
    [[nodiscard]] QVector<PlaybackLogEntry> allEntries() const;
    [[nodiscard]] QVector<PlaybackLogEntry> recentEntries(int count = 100) const;
    [[nodiscard]] QVector<PlaybackLogEntry> entriesSince(const QDateTime& since) const;
    [[nodiscard]] QVector<PlaybackLogEntry> entriesForCue(const QString& cueId) const;

    // export
    [[nodiscard]] bool exportToFile(const QString& path) const;
    [[nodiscard]] bool exportToCSV(const QString& path) const;

    // clear logs
    void clear();

  signals:
    void entryAdded(const PlaybackLogEntry& entry);
    void logCleared();

  private:
    void addEntry(const PlaybackLogEntry& entry);
    void writeToFile(const PlaybackLogEntry& entry);
    void pruneOldEntries();
    [[nodiscard]] QVector<PlaybackLogEntry>
    entriesMatching(std::function<bool(const PlaybackLogEntry&)> predicate) const;

    static constexpr int DEFAULT_MAX_MEMORY_ENTRIES = 10000;

    mutable QMutex m_mutex;
    QVector<PlaybackLogEntry> m_entries;
    int m_maxMemoryEntries = DEFAULT_MAX_MEMORY_ENTRIES;

    QString m_logFilePath;
    QFile m_logFile;
};

} // namespace OpenMix
