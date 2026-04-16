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
    static LogEntry fromJson(const QJsonObject& json);
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
    QString logFilePath() const { return m_logFilePath; }
    bool isFileLoggingEnabled() const { return m_logFile.isOpen(); }
    void closeLogFile();

    // memory management
    void setMaxMemoryEntries(int max) { m_maxMemoryEntries = max; }
    int maxMemoryEntries() const { return m_maxMemoryEntries; }

    // batch emission interval for UI updates (ms)
    void setBatchInterval(int ms);
    int batchInterval() const { return m_batchInterval; }

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
    QVector<LogEntry> allEntries() const;
    QVector<LogEntry> recentEntries(int count = 100) const;
    QVector<LogEntry> entriesSince(const QDateTime& since) const;
    QVector<LogEntry> entriesFiltered(LogLevel minLevel = LogLevel::Debug,
                                      LogSource source = LogSource::System,
                                      bool filterBySource = false) const;

    // export
    bool exportToFile(const QString& path) const;
    bool exportToCSV(const QString& path) const;

    // clear logs
    void clear();

  signals:
    void entryAdded(const LogEntry& entry);
    void entriesBatchAdded(const QVector<LogEntry>& entries);
    void logCleared();

  private slots:
    void flushBatch();

  private:
    void addEntry(const LogEntry& entry);
    void writeToFile(const LogEntry& entry);
    void pruneOldEntries();

    mutable QMutex m_mutex;
    QVector<LogEntry> m_entries;
    int m_maxMemoryEntries = 5000;

    QString m_logFilePath;
    QFile m_logFile;

    // batched emission for UI performance
    QTimer* m_batchTimer;
    QVector<LogEntry> m_pendingBatch;
    int m_batchInterval = 100; // ms
};

} // namespace OpenMix
