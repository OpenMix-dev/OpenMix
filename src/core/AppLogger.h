#pragma once

#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace OpenMix {

enum class LogLevel { Debug, Info, Warning, Error, Critical };

enum class LogSource { Connection, Protocol, Playback, UI, System, MIDI, Discovery };

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    LogSource source;
    QString message;
    QJsonObject metadata;

    QString levelString() const;
    QString sourceString() const;
    QJsonObject toJson() const;
    [[nodiscard]] static LogEntry fromJson(const QJsonObject& json);
    static LogLevel levelFromString(const QString& str);
    static LogSource sourceFromString(const QString& str);
};

class AppLogger : public QObject {
    Q_OBJECT

  public:
    explicit AppLogger(QObject* parent = nullptr);
    ~AppLogger() override;

    // file logging
    void setLogFile(const QString& path);
    [[nodiscard]] QString logFilePath() const { return m_logFilePath; }
    [[nodiscard]] bool isFileLoggingEnabled() const { return m_logFile.isOpen(); }
    void closeLogFile();

    // memory management
    void setMaxMemoryEntries(int max) { m_maxMemoryEntries = max; }
    [[nodiscard]] int maxMemoryEntries() const noexcept { return m_maxMemoryEntries; }

    // batch emission interval for UI updates (ms)
    void setBatchInterval(int ms);
    [[nodiscard]] int batchInterval() const noexcept { return m_batchInterval; }

    // core logging method
    void log(LogLevel level, LogSource source, const QString& message,
             const QJsonObject& metadata = {});

    // convenience methods by level
    void debug(LogSource source, const QString& message, const QJsonObject& metadata = {});
    void info(LogSource source, const QString& message, const QJsonObject& metadata = {});
    void warning(LogSource source, const QString& message, const QJsonObject& metadata = {});
    void error(LogSource source, const QString& message, const QJsonObject& metadata = {});
    void critical(LogSource source, const QString& message, const QJsonObject& metadata = {});

    // connection-specific convenience methods
    void logConnectionAttempt(const QString& protocol, const QString& host, int port);
    void logConnectionSuccess(const QString& protocol, const QString& host, int port);
    void logConnectionFailed(const QString& protocol, const QString& host, int port,
                             const QString& error);
    void logConnectionLost(const QString& protocol, const QString& host, int port);
    void logReconnectAttempt(const QString& protocol, const QString& host, int port, int attempt,
                             int maxAttempts);
    void logDisconnected(const QString& protocol, const QString& host, int port);

    // retrieve entries
    [[nodiscard]] QVector<LogEntry> allEntries() const;
    [[nodiscard]] QVector<LogEntry> recentEntries(int count = 100) const;
    [[nodiscard]] QVector<LogEntry> entriesSince(const QDateTime& since) const;
    [[nodiscard]] QVector<LogEntry> entriesFiltered(LogLevel minLevel = LogLevel::Debug,
                                                    LogSource source = LogSource::System,
                                                    bool filterBySource = false) const;

    // export
    [[nodiscard]] bool exportToFile(const QString& path) const;
    [[nodiscard]] bool exportToCSV(const QString& path) const;

    // clear logs
    void clear();

  signals:
    void entryAdded(const LogEntry& entry);
    void entriesBatchAdded(const QVector<LogEntry>& entries);
    void logCleared();

  private slots:
    void flushBatch();

  private:
    void logConnectionEvent(LogLevel level, const QString& event, const QJsonObject& metadata);
    void addEntry(const LogEntry& entry);
    void writeToFile(const LogEntry& entry);
    void pruneOldEntries();

    static constexpr int DEFAULT_MAX_MEMORY_ENTRIES = 5000;
    static constexpr int DEFAULT_BATCH_INTERVAL_MS = 100;

    mutable QMutex m_mutex;
    QVector<LogEntry> m_entries;
    int m_maxMemoryEntries = DEFAULT_MAX_MEMORY_ENTRIES;

    QString m_logFilePath;
    QFile m_logFile;

    // batched emission for UI performance
    QTimer* m_batchTimer;
    QVector<LogEntry> m_pendingBatch;
    int m_batchInterval = DEFAULT_BATCH_INTERVAL_MS;
};

} // namespace OpenMix
